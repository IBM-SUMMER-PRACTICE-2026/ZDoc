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

/**
 * @brief Open a directory for iteration (Windows back end).
 *
 * Appends a "\*" wildcard to disk_path and starts a FindFirstFileA search,
 * leaving out primed so the first fs_dir_next call returns that initial
 * result instead of calling FindNextFileA.
 *
 * @param disk_path Directory to open, without a trailing wildcard.
 * @param out Iteration handle to initialise.
 * @return 0 on success, -1 if the search path overflowed or the search
 *         could not be started.
 */
static int fs_dir_open(const char* disk_path, fs_dir_t* out) {
    char search_path[32768 + 8];
    int n = snprintf(search_path, sizeof(search_path), "%s\\*", disk_path);
    if (n < 0 || (size_t)n >= sizeof(search_path)) return -1;

    out->handle = FindFirstFileA(search_path, &out->find_data);
    if (out->handle == INVALID_HANDLE_VALUE) return -1;
    out->first_call = 1;
    return 0;
}

/**
 * @brief Resolve a root path to its Windows extended-length ("\\?\") form.
 *
 * Normalises root_disk_path with GetFullPathNameA and prefixes it with
 * "\\?\" (unless it is already so prefixed) so subsequent directory
 * operations are not bound by MAX_PATH.
 *
 * @param root_disk_path Root path as given by the caller.
 * @param out Buffer that receives the extended-length path.
 * @param out_size Size in bytes of out.
 * @return 0 on success, -1 if resolution failed or out was too small.
 */
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

/**
 * @brief Advance to the next directory entry (Windows back end).
 *
 * Consumes the primed result from fs_dir_open on the first call, then calls
 * FindNextFileA on subsequent calls; "." and ".." entries are skipped
 * transparently.
 *
 * @param dir Iteration handle previously opened with fs_dir_open.
 * @param name_out Buffer that receives the entry's bare name.
 * @param name_out_size Size in bytes of name_out.
 * @param is_directory Set to nonzero if the entry is a directory, zero otherwise.
 * @return 1 with name_out/is_directory filled in, 0 when done.
 */
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

/**
 * @brief Close a directory iteration handle (Windows back end).
 *
 * @param dir Iteration handle previously opened with fs_dir_open.
 */
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

/**
 * @brief Open a directory for iteration (POSIX back end).
 *
 * Opens disk_path with opendir and records it in out->base_path so
 * fs_dir_next can stat each entry by its full path.
 *
 * @param disk_path Directory to open.
 * @param out Iteration handle to initialise.
 * @return 0 on success, -1 if the directory could not be opened.
 */
static int fs_dir_open(const char* disk_path, fs_dir_t* out) {
    out->handle = opendir(disk_path);
    if (!out->handle) return -1;
    snprintf(out->base_path, sizeof(out->base_path), "%s", disk_path);
    return 0;
}

/**
 * @brief Advance to the next directory entry (POSIX back end).
 *
 * Calls readdir, skipping "." and "..", then stats the entry's full path
 * (base_path joined with the entry name) to determine whether it is a
 * directory.
 *
 * @param dir Iteration handle previously opened with fs_dir_open.
 * @param name_out Buffer that receives the entry's bare name.
 * @param name_out_size Size in bytes of name_out.
 * @param is_directory Set to nonzero if the entry is a directory, zero otherwise.
 * @return 1 with name_out/is_directory filled in, 0 when done, -1 on error
 *         (full path overflow or stat failure).
 */
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

/**
 * @brief Close a directory iteration handle (POSIX back end).
 *
 * @param dir Iteration handle previously opened with fs_dir_open.
 */
static void fs_dir_close(fs_dir_t* dir) {
    closedir(dir->handle);
}

#endif

/* ============================================================
 * Shared logic — identical on every platform.
 * ============================================================ */

/**
 * @brief Check whether a file name's extension matches one of a filter list.
 *
 * If extension_count is 0, every file matches (no filtering).
 *
 * @param name File name to test.
 * @param extensions Array of extension strings to match against, each
 *                    including the leading dot (e.g. ".plx").
 * @param extension_count Number of entries in extensions.
 * @return 1 if name's extension matches one of extensions (or filtering is
 *         disabled), 0 otherwise.
 */
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

/**
 * @brief Simple shell-style wildcard match.
 *
 * '*' matches any run of characters (including none, including path
 * separators), '?' matches exactly one character. Not a full
 * gitignore-style segment-aware "**" glob, but covers the common exclude
 * patterns (e.g. "*.test.c" or a bare directory name like "test").
 *
 * @param pat Wildcard pattern.
 * @param str String to test against pat.
 * @return 1 if str matches pat in its entirety, 0 otherwise.
 */
static int wild_match(const char* pat, const char* str) {
    const char* star_pat = NULL;
    const char* star_str = NULL;

    while (*str) {
        if (*pat == '*') {
            star_pat = ++pat;
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

/**
 * @brief Check whether an entry matches any of the caller's exclude patterns.
 *
 * An entry is excluded if any pattern matches either its bare name or its
 * accumulated disk path from the walk root.
 *
 * @param name Entry's bare name.
 * @param disk_path Entry's accumulated disk path from the walk root.
 * @param excludes Array of wild_match glob patterns.
 * @param exclude_count Number of entries in excludes.
 * @return 1 if the entry is excluded, 0 otherwise.
 */
static int is_excluded(const char* name, const char* disk_path,
                        const char** excludes, size_t exclude_count) {
    for (size_t i = 0; i < exclude_count; i++) {
        if (wild_match(excludes[i], name) || wild_match(excludes[i], disk_path)) return 1;
    }
    return 0;
}

/**
 * @brief Recursively walk one directory, interning its matching entries.
 *
 * Iterates disk_path's entries, skipping any that match excludes. Files
 * whose extension matches extensions are interned into files under
 * current_dir_index. Subdirectories are interned into dirs and, when
 * recursive is nonzero, descended into with a fresh recursive call;
 * when recursive is 0, subdirectories are skipped entirely (neither
 * interned nor descended into).
 *
 * @param disk_path Disk path of the directory to walk.
 * @param current_dir_index Index in dirs of disk_path itself, used as the
 *                           parent for any entries interned here.
 * @param dirs Directory table to intern subdirectories into.
 * @param files File table to intern matching files into.
 * @param extensions Array of extension strings to filter files by.
 * @param extension_count Number of entries in extensions; 0 matches every file.
 * @param excludes Array of wild_match glob patterns to skip entries by.
 * @param exclude_count Number of entries in excludes.
 * @param recursive When 0, subdirectories are neither interned nor descended into.
 * @return ZDOC_OK on success, ZDOC_FS_WALK_FAILED if the directory could not
 *         be opened or read, ZDOC_OUT_OF_MEMORY if interning failed, or
 *         ZDOC_PATH_TOO_LONG if a child path overflowed its buffer.
 */
static enum ZDoc_Error walk_dir(const char* disk_path, int current_dir_index,
                     modtree_dir_table_t* dirs, modtree_file_table_t* files,
                     const char** extensions, size_t extension_count,
                     const char** excludes, size_t exclude_count, int recursive) {
    fs_dir_t dir;
    if (fs_dir_open(disk_path, &dir) != 0) return ZDOC_FS_WALK_FAILED;

    char name[256];
    int is_directory;
    int result;

    while ((result = fs_dir_next(&dir, name, sizeof(name), &is_directory)) == 1) {
        char child_disk_path[FS_WALK_PATH_BUF];
        int n = snprintf(child_disk_path, sizeof(child_disk_path), "%s" FS_WALK_SEP "%s", disk_path, name);
        if (n < 0 || (size_t)n >= sizeof(child_disk_path)) { fs_dir_close(&dir); return ZDOC_PATH_TOO_LONG; }

        if (is_excluded(name, child_disk_path, excludes, exclude_count)) continue;

        if (is_directory) {
            if (!recursive) continue; /* not interned, not descended into */

            int new_index = modtree_intern_dir(dirs, name, current_dir_index);
            if (new_index < 0) { fs_dir_close(&dir); return ZDOC_OUT_OF_MEMORY; }

            enum ZDoc_Error sub_status = walk_dir(child_disk_path, new_index, dirs, files,
                         extensions, extension_count, excludes, exclude_count, recursive);
            if (sub_status != ZDOC_OK) {
                fs_dir_close(&dir);
                return sub_status;
            }
        } else if (matches_extension(name, extensions, extension_count)) {
            if (modtree_intern_file(files, name, current_dir_index) < 0) {
                fs_dir_close(&dir);
                return ZDOC_OUT_OF_MEMORY;
            }
        }
    }

    fs_dir_close(&dir);
    return (result == -1) ? ZDOC_FS_WALK_FAILED : ZDOC_OK;
}

/**
 * @brief Extract the last path component of a disk path.
 *
 * E.g. "/tmp/myproject" -> "myproject", "C:\\src\\myproject" -> "myproject".
 * Falls back to the whole string if no separator is found.
 *
 * @param disk_path Path to extract the last component from.
 * @return Pointer into disk_path at the start of its last component.
 */
static const char* last_path_component(const char* disk_path) {
    const char* slash = strrchr(disk_path, '/');
    const char* backslash = strrchr(disk_path, '\\');
    const char* last = disk_path;
    if (slash && slash + 1 > last) last = slash + 1;
    if (backslash && backslash + 1 > last) last = backslash + 1;
    return last;
}

/**
 * @brief Walk a directory tree, interning matching directories and files.
 *
 * Resolves root_disk_path to an absolute path, records its parent in
 * fs_walk_root_prefix, seeds the root directory itself into dirs (its own
 * name, parent = -1), then walks its contents via walk_dir. On Windows the
 * walk uses the extended-length ("\\?\") form of the root so it is not
 * bound by MAX_PATH.
 *
 * @param root_disk_path Disk path of the directory to walk.
 * @param dirs Directory table to intern subdirectories into.
 * @param files File table to intern matching files into.
 * @param extensions Array of extension strings, each including the leading
 *                    dot (e.g. ".plx", ".pli"); comparison is case-sensitive.
 * @param extension_count Number of entries in extensions; 0 (extensions may
 *                         then be NULL) matches every file.
 * @param excludes Glob patterns checked against both the bare entry name
 *                 and the accumulated disk path; a match excludes the
 *                 entry entirely.
 * @param exclude_count Number of entries in excludes; 0 (excludes may then
 *                       be NULL) excludes nothing.
 * @param recursive When 0, only root_disk_path's immediate file entries are
 *                  considered; subdirectories are neither interned nor
 *                  descended into. When non-zero, the walk descends into
 *                  every non-excluded subdirectory.
 * @return ZDOC_OK on success. ZDOC_FS_WALK_FAILED if root_disk_path (or a
 *         directory beneath it) could not be resolved, opened or read;
 *         ZDOC_OUT_OF_MEMORY if interning a directory or file failed; or
 *         ZDOC_PATH_TOO_LONG if a reconstructed disk path overflowed its buffer.
 */
enum ZDoc_Error fs_walk(const char* root_disk_path,
            modtree_dir_table_t* dirs,
            modtree_file_table_t* files,
            const char** extensions,
            size_t extension_count,
            const char** excludes,
            size_t exclude_count,
            int recursive) {
    char abs_root[FS_WALK_PATH_MAX];
    if (resolve_absolute_path(root_disk_path, abs_root, sizeof(abs_root)) != 0) return ZDOC_FS_WALK_FAILED;

    const char* root_name = last_path_component(abs_root);

    snprintf(fs_walk_root_prefix, sizeof(fs_walk_root_prefix), "%s", abs_root);
    char* fsep = strrchr(fs_walk_root_prefix, '/');
    char* bsep = strrchr(fs_walk_root_prefix, '\\');
    char* sep = fsep > bsep ? fsep : bsep;
    if (sep && sep != fs_walk_root_prefix) *sep = '\0';

    int root_index = modtree_intern_dir(dirs, root_name, -1);
    if (root_index < 0) return ZDOC_OUT_OF_MEMORY;

#ifdef _WIN32
    char prefixed_root[FS_WALK_PATH_BUF];
    if (win32_root_prefixed(root_disk_path, prefixed_root, sizeof(prefixed_root)) != 0) return ZDOC_PATH_TOO_LONG;
    return walk_dir(prefixed_root, root_index, dirs, files, extensions, extension_count,
                     excludes, exclude_count, recursive);
#else
    return walk_dir(root_disk_path, root_index, dirs, files, extensions, extension_count,
                     excludes, exclude_count, recursive);
#endif
}