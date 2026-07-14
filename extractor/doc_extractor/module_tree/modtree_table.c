#include "modtree_tables.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MODTREE_PATH_BUF 4096

static char *modtree_strdup(const char *src) {
    size_t len = strlen(src) + 1;
    char *copy = malloc(len);
    if (!copy) return NULL;
    memcpy(copy, src, len);
    return copy;
}

/* ---------- lifecycle ---------- */
void modtree_dir_table_init(modtree_dir_table_t* t) {
    t->dirs = NULL;
    t->capacity = 0;
    t->count = 0;
}

void modtree_dir_table_free(modtree_dir_table_t* t) {
    for (size_t i = 0; i < t->count; i++) {
        free(t->dirs[i].name);
    }
    free(t->dirs);
    t->dirs = NULL;
    t->count = 0;
    t->capacity = 0;
}

void modtree_file_table_init(modtree_file_table_t* t) {
    t->files = NULL;
    t->count = 0;
    t->capacity = 0;
}
 
void modtree_file_table_free(modtree_file_table_t* t) {
    for (size_t i = 0; i < t->count; i++) {
        free(t->files[i].name);
    }
    free(t->files);
    t->files = NULL;
    t->count = 0;
    t->capacity = 0;
}

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

/* Recursive helper: builds the full path for dir_index into buf.
 * Returns ZDOC_OK on success, ZDOC_PATH_TOO_LONG on overflow. */
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

int modtree_dir_path(const modtree_dir_table_t* t, int dir_index, char* out, size_t out_size) {
    return (build_dir_path(t, dir_index, out, out_size) == ZDOC_OK) ? 0 : -1;
}

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

int number_of_files(const modtree_file_table_t* t) {
    if (!t) return -1;

    return t->count;
}