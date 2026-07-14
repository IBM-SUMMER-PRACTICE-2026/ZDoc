#include "modtree_tables.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MODTREE_PATH_BUF 4096

/**
 * @brief Duplicate a NUL-terminated string onto the heap.
 *
 * @param src String to copy.
 * @return Newly allocated copy of src, or NULL if allocation failed.
 */
static char *modtree_strdup(const char *src) {
    size_t len = strlen(src) + 1;
    char *copy = malloc(len);
    if (!copy) return NULL;
    memcpy(copy, src, len);
    return copy;
}

/* ---------- lifecycle ---------- */
/**
 * @brief Initialise an empty directory table.
 *
 * @param t Directory table to initialise.
 */
void modtree_dir_table_init(modtree_dir_table_t* t) {
    t->dirs = NULL;
    t->capacity = 0;
    t->count = 0;
}

/**
 * @brief Release a directory table and every name it owns.
 *
 * Frees each interned directory's name string, then the backing array
 * itself, and resets t to an empty, reusable state.
 *
 * @param t Directory table to free.
 */
void modtree_dir_table_free(modtree_dir_table_t* t) {
    for (size_t i = 0; i < t->count; i++) {
        free(t->dirs[i].name);
    }
    free(t->dirs);
    t->dirs = NULL;
    t->count = 0;
    t->capacity = 0;
}

/**
 * @brief Initialise an empty file table.
 *
 * @param t File table to initialise.
 */
void modtree_file_table_init(modtree_file_table_t* t) {
    t->files = NULL;
    t->count = 0;
    t->capacity = 0;
}

/**
 * @brief Release a file table and every name it owns.
 *
 * Frees each interned file's name string, then the backing array itself,
 * and resets t to an empty, reusable state.
 *
 * @param t File table to free.
 */
void modtree_file_table_free(modtree_file_table_t* t) {
    for (size_t i = 0; i < t->count; i++) {
        free(t->files[i].name);
    }
    free(t->files);
    t->files = NULL;
    t->count = 0;
    t->capacity = 0;
}

/**
 * @brief Intern a new directory entry, growing the table if needed.
 *
 * No de-duplication is performed - a tree walk never revisits the same
 * directory, so every call appends a new entry.
 *
 * @param t Directory table to insert into.
 * @param name Directory's own (bare) name; copied into the table.
 * @param parent_index Index of the parent directory in t, or -1 for a root.
 * @return Index of the newly inserted entry, or -1 if growing the table or
 *         copying name failed.
 */
int modtree_intern_dir(modtree_dir_table_t* t, const char* name, int parent_index) {
    if (t->count == t->capacity) {
        size_t new_capacity = t->capacity ? t->capacity * 2 : 64;
        modtree_dir_t* grown = realloc(t->dirs, new_capacity * sizeof(modtree_dir_t));
        if (!grown) return -1;
        t->dirs = grown;
        t->capacity = new_capacity;
    }

    int index = (int)t->count;
    t->dirs[index].name = modtree_strdup(name);
    if (!t->dirs[index].name) return -1;
    t->dirs[index].parent_index = parent_index;
    t->count++;
    return index;
}

/**
 * @brief Intern a new file entry, growing the table if needed.
 *
 * No de-duplication is performed - a tree walk never revisits the same
 * file, so every call appends a new entry.
 *
 * @param t File table to insert into.
 * @param name File's own (bare) name; copied into the table.
 * @param parent_dir_index Index of the containing directory in the
 *                          corresponding directory table.
 * @return Index of the newly inserted entry, or -1 if growing the table or
 *         copying name failed.
 */
int modtree_intern_file(modtree_file_table_t* t, const char* name, int parent_dir_index) {
    if (t->count == t->capacity) {
        size_t new_capacity = t->capacity ? t->capacity * 2 : 64;
        modtree_file_t* grown = realloc(t->files, new_capacity * sizeof(modtree_file_t));
        if (!grown) return -1;
        t->files = grown;
        t->capacity = new_capacity;
    }

    int index = (int)t->count;
    t->files[index].name = modtree_strdup(name);
    if (!t->files[index].name) return -1;
    t->files[index].parent_dir_index = parent_dir_index;
    t->count++;
    return index;
}

/* ---------- path reconstruction ---------- */

/**
 * @brief Recursively build the full relative path for a directory index.
 *
 * Walks parent_index links from dir_index up to a root (index -1), then
 * joins each directory's name back together with '/' on the way down.
 *
 * @param t Directory table dir_index refers into.
 * @param dir_index Index of the directory to build the path for, or -1 for
 *                   "no directory" (writes an empty string).
 * @param buf Buffer that receives the reconstructed path.
 * @param buf_size Size in bytes of buf.
 * @return ZDOC_OK on success, ZDOC_PATH_TOO_LONG if buf was too small.
 */
static enum ZDoc_Error build_dir_path(const modtree_dir_table_t* t, int dir_index, char* buf, size_t buf_size) {
    if (dir_index == -1) {
        buf[0] = '\0';
        return ZDOC_OK;
    }

    char parent_buf[MODTREE_PATH_BUF];
    enum ZDoc_Error parent_status = build_dir_path(t, t->dirs[dir_index].parent_index, parent_buf, sizeof(parent_buf));
    if (parent_status != ZDOC_OK) return parent_status;

    int written;
    if (parent_buf[0] == '\0') {
        written = snprintf(buf, buf_size, "%s", t->dirs[dir_index].name);
    } else {
        written = snprintf(buf, buf_size, "%s/%s", parent_buf, t->dirs[dir_index].name);
    }

    if (written < 0 || (size_t)written >= buf_size) return ZDOC_PATH_TOO_LONG;
    return ZDOC_OK;
}

/**
 * @brief Reconstruct the full relative path for a directory.
 *
 * @param t Directory table dir_index refers into.
 * @param dir_index Index of the directory to build the path for.
 * @param out Buffer that receives the reconstructed path (e.g. "folder1/folder2").
 * @param out_size Size in bytes of out.
 * @return 0 on success, -1 if out was too small.
 */
int modtree_dir_path(const modtree_dir_table_t* t, int dir_index, char* out, size_t out_size) {
    return (build_dir_path(t, dir_index, out, out_size) == ZDOC_OK) ? 0 : -1;
}

/**
 * @brief Reconstruct the full relative path for a file.
 *
 * @param dirs Directory table that the file's parent directory lives in.
 * @param files File table file_index refers into.
 * @param file_index Index of the file to build the path for.
 * @param out Buffer that receives the reconstructed path
 *            (e.g. "folder1/folder2/file1.plx").
 * @param out_size Size in bytes of out.
 * @return ZDOC_OK on success, ZDOC_PATH_TOO_LONG if out was too small.
 */
enum ZDoc_Error modtree_file_path(const modtree_dir_table_t* dirs, const modtree_file_table_t* files,
                       int file_index, char* out, size_t out_size) {
    char dir_buf[MODTREE_PATH_BUF];
    int parent_dir_index = files->files[file_index].parent_dir_index;

    enum ZDoc_Error dir_status = build_dir_path(dirs, parent_dir_index, dir_buf, sizeof(dir_buf));
    if (dir_status != ZDOC_OK) return dir_status;

    int written;
    if (dir_buf[0] == '\0') {
        written = snprintf(out, out_size, "%s", files->files[file_index].name);
    } else {
        written = snprintf(out, out_size, "%s/%s", dir_buf, files->files[file_index].name);
    }

    if (written < 0 || (size_t)written >= out_size) return ZDOC_PATH_TOO_LONG;
    return ZDOC_OK;
}

/**
 * @brief Report how many files are currently interned in a file table.
 *
 * @param t File table to query.
 * @return Number of interned files, or -1 if t is NULL.
 */
int number_of_files(const modtree_file_table_t* t) {
    if (!t) return -1;

    return t->count;
}