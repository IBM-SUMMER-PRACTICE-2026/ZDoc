#include "fs_walk.h"
#include "path_interface.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define FS_WALK_PATH_BUF 4096

char fs_walk_root_prefix[FS_WALK_PATH_MAX] = {0};

#ifdef _WIN32
#define FS_WALK_SEP "\\"
#else
#define FS_WALK_SEP "/"
#endif

/* ============================================================
 * Platform-specific directory iteration primitives.
 * Everything below this block (matches_extension, walk_dir,
 * fs_walk) is identical for every platform and contains no
 * #ifdef at all.
 * ============================================================ */

#ifdef _WIN32

#include <windows.h>

typedef struct {
    HANDLE handle;
    WIN32_FIND_DATAA find_data;
    int first_call;
} fs_dir_t;

static int fs_dir_open(const char* disk_path, fs_dir_t* out) {
    char search_path[32768 + 8];
    int n = snprintf(search_path, sizeof(search_path), "%s\\*", disk_path);
    if (n < 0 || (size_t)n >= sizeof(search_path)) return -1;

    out->handle = FindFirstFileA(search_path, &out->find_data);
    if (out->handle == INVALID_HANDLE_VALUE) return -1;
    out->first_call = 1;
    return 0;
}

static int win32_root_prefixed(const char* root_disk_path, char* out, size_t out_size) {
    char full_path[32768];
    DWORD full_len = GetFullPathNameA(root_disk_path, sizeof(full_path), full_path, NULL);
    if (full_len == 0 || full_len >= sizeof(full_path)) return -1;

    int n;
    if (strncmp(full_path, "\\\\?\\", 4) == 0) {
        n = snprintf(out, out_size, "%s", full_path);
    } else {
        n = snprintf(out, out_size, "\\\\?\\%s", full_path);
    }
    if (n < 0 || (size_t)n >= out_size) return -1;
    return 0;
}

/* Returns 1 with name/is_directory filled in, 0 when done. */
static int fs_dir_next(fs_dir_t* dir, char* name_out, size_t name_out_size, int* is_directory) {
    for (;;) {
        if (!dir->first_call) {
            if (!FindNextFileA(dir->handle, &dir->find_data)) return 0;
        }
        dir->first_call = 0;

        if (strcmp(dir->find_data.cFileName, ".") == 0 ||
            strcmp(dir->find_data.cFileName, "..") == 0)
            continue;

        snprintf(name_out, name_out_size, "%s", dir->find_data.cFileName);
        *is_directory = (dir->find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        return 1;
    }
}

static void fs_dir_close(fs_dir_t* dir) {
    FindClose(dir->handle);
}

#else /* POSIX: Linux, macOS, BSD */

#include <dirent.h>
#include <sys/stat.h>

typedef struct {
    DIR* handle;
    char base_path[FS_WALK_PATH_BUF];
} fs_dir_t;

static int fs_dir_open(const char* disk_path, fs_dir_t* out) {
    out->handle = opendir(disk_path);
    if (!out->handle) return -1;
    snprintf(out->base_path, sizeof(out->base_path), "%s", disk_path);
    return 0;
}

/* Returns 1 with name/is_directory filled in, 0 when done, -1 on error. */
static int fs_dir_next(fs_dir_t* dir, char* name_out, size_t name_out_size, int* is_directory) {
    struct dirent* e;
    while ((e = readdir(dir->handle)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;

        snprintf(name_out, name_out_size, "%s", e->d_name);

        char full[FS_WALK_PATH_BUF];
        int n = snprintf(full, sizeof(full), "%s/%s", dir->base_path, e->d_name);
        if (n < 0 || (size_t)n >= sizeof(full)) return -1;

        struct stat st;
        if (stat(full, &st) != 0) return -1;

        *is_directory = S_ISDIR(st.st_mode);
        return 1;
    }
    return 0;
}

static void fs_dir_close(fs_dir_t* dir) {
    closedir(dir->handle);
}

#endif

/* ============================================================
 * Shared logic — identical on every platform.
 * ============================================================ */

/* Returns 1 if name's extension matches one of extensions, 0 otherwise.
 * If extension_count is 0, every file matches (no filtering). */
static int matches_extension(const char* name, const char** extensions, size_t extension_count) {
    if (extension_count == 0) return 1;

    size_t name_len = strlen(name);
    for (size_t i = 0; i < extension_count; i++) {
        size_t ext_len = strlen(extensions[i]);
        if (name_len < ext_len) continue;
        if (strcmp(name + (name_len - ext_len), extensions[i]) == 0) return 1;
    }
    return 0;
}

/* Simple shell-style wildcard match: '*' matches any run of characters
 * (including none, including path separators), '?' matches exactly one
 * character. Not a full gitignore-style segment-aware "**" glob, but
 * covers the common exclude patterns (e.g. "*.test.c" or a bare directory
 * name like "test"). */
static int wild_match(const char* pat, const char* str) {
    const char* star_pat = NULL;
    const char* star_str = NULL;

    while (*str) {
        if (*pat == '*') {
            star_str = str;
        } else if (*pat == '?' || *pat == *str) {
            pat++;
            str++;
        } else if (star_pat) {
            pat = star_pat;
            str = ++star_str;
        } else {
            return 0;
        }
    }
    while (*pat == '*') pat++;
    return *pat == '\0';
}

/* An entry is excluded if any pattern matches either its bare name or its
 * accumulated disk path from the walk root. */
static int is_excluded(const char* name, const char* disk_path,
                        const char** excludes, size_t exclude_count) {
    for (size_t i = 0; i < exclude_count; i++) {
        if (wild_match(excludes[i], name) || wild_match(excludes[i], disk_path)) return 1;
    }
    return 0;
}

static int walk_dir(const char* disk_path, int current_dir_index,
                     modtree_dir_table_t* dirs, modtree_file_table_t* files,
                     const char** extensions, size_t extension_count,
                     const char** excludes, size_t exclude_count, int recursive) {
    fs_dir_t dir;
    if (fs_dir_open(disk_path, &dir) != 0) return -1;

    char name[256];
    int is_directory;
    int result;

    while ((result = fs_dir_next(&dir, name, sizeof(name), &is_directory)) == 1) {
        char child_disk_path[FS_WALK_PATH_BUF];
        int n = snprintf(child_disk_path, sizeof(child_disk_path), "%s" FS_WALK_SEP "%s", disk_path, name);
        if (n < 0 || (size_t)n >= sizeof(child_disk_path)) { fs_dir_close(&dir); return -1; }

        if (is_excluded(name, child_disk_path, excludes, exclude_count)) continue;

        if (is_directory) {
            if (!recursive) continue; /* not interned, not descended into */

            int new_index = modtree_intern_dir(dirs, name, current_dir_index);
            if (new_index < 0) { fs_dir_close(&dir); return -1; }

            if (walk_dir(child_disk_path, new_index, dirs, files, extensions, extension_count,
                         excludes, exclude_count, recursive) != 0) {
                fs_dir_close(&dir);
                return -1;
            }
        } else if (matches_extension(name, extensions, extension_count)) {
            if (modtree_intern_file(files, name, current_dir_index) < 0) {
                fs_dir_close(&dir);
                return -1;
            }
        }
    }

    fs_dir_close(&dir);
    return (result == -1) ? -1 : 0;
}

/* Extracts the last path component of disk_path, e.g.
 * "/tmp/myproject" -> "myproject", "C:\\src\\myproject" -> "myproject".
 * Falls back to the whole string if no separator is found. */
static const char* last_path_component(const char* disk_path) {
    const char* slash = strrchr(disk_path, '/');
    const char* backslash = strrchr(disk_path, '\\');
    const char* last = disk_path;
    if (slash && slash + 1 > last) last = slash + 1;
    if (backslash && backslash + 1 > last) last = backslash + 1;
    return last;
}

int fs_walk(const char* root_disk_path,
            modtree_dir_table_t* dirs,
            modtree_file_table_t* files,
            const char** extensions,
            size_t extension_count,
            const char** excludes,
            size_t exclude_count,
            int recursive) {
    char abs_root[FS_WALK_PATH_MAX];
    if (resolve_absolute_path(root_disk_path, abs_root, sizeof(abs_root)) != 0) return -1;

    const char* root_name = last_path_component(abs_root);

    snprintf(fs_walk_root_prefix, sizeof(fs_walk_root_prefix), "%s", abs_root);
    char* fsep = strrchr(fs_walk_root_prefix, '/');
    char* bsep = strrchr(fs_walk_root_prefix, '\\');
    char* sep = fsep > bsep ? fsep : bsep;
    if (sep && sep != fs_walk_root_prefix) *sep = '\0';

    int root_index = modtree_intern_dir(dirs, root_name, -1);
    if (root_index < 0) return -1;

#ifdef _WIN32
    char prefixed_root[FS_WALK_PATH_BUF];
    if (win32_root_prefixed(root_disk_path, prefixed_root, sizeof(prefixed_root)) != 0) return -1;
    return walk_dir(prefixed_root, root_index, dirs, files, extensions, extension_count,
                     excludes, exclude_count, recursive);
#else
    return walk_dir(root_disk_path, root_index, dirs, files, extensions, extension_count,
                     excludes, exclude_count, recursive);
#endif
}