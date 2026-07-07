#ifndef ZDOC_MODTREE_TABLES_H
#define ZDOC_MODTREE_TABLES_H

#include <stddef.h>
#include <assert.h>
#include <stdint.h>

/* ---------- directory table ----------
 * Each directory stores only its own name plus the index of its parent
 * directory in this same table. The root's parent is -1. Full paths are
 * never stored - they're reconstructed on demand by walking parent links.
 */
typedef struct {
    char* name;
    int   parent_index;
    uint8_t _pad[4];
} modtree_dir_t;

typedef struct {
    modtree_dir_t* dirs;
    size_t         count;
    size_t         capacity;
} modtree_dir_table_t;

static_assert(sizeof(modtree_dir_t) == 16, "modtree_dir_t layout changed unexpectedly");

/* ---------- file table ----------
 * Each file stores only its own name plus the index of the directory
 * (in modtree_dir_table_t) it lives in.
 */
typedef struct {
    char*   name;
    int     parent_dir_index;
    uint8_t _pad[4];
} modtree_file_t;

static_assert(sizeof(modtree_file_t) == 16, "modtree_file_t layout changed unexpectedly");

typedef struct {
    modtree_file_t *files;
    size_t count;
    size_t capacity;
} modtree_file_table_t;

/* Lifecycle */
void modtree_dir_table_init(modtree_dir_table_t* t);
void modtree_dir_table_free(modtree_dir_table_t* t);
void modtree_file_table_init(modtree_file_table_t* t);
void modtree_file_table_free(modtree_file_table_t* t);

/* Insertion. No de-duplication — a tree walk never revisits the same
 * directory or file, so no lookup/hash step is needed. Returns the new
 * entry's index. */
int modtree_intern_dir(modtree_dir_table_t* t, const char* name, int parent_index);
int modtree_intern_file(modtree_file_table_t* t, const char* name, int parent_dir_index);

/* Reconstructs the full relative path for a directory (e.g. "folder1/folder2")
 * into out. Returns 0 on success, -1 if out was too small. */
int modtree_dir_path(const modtree_dir_table_t* t, int dir_index, char* out, size_t out_size);

/* Reconstructs the full relative path for a file (e.g. "folder1/folder2/file1.plx")
 * into out. Returns 0 on success, -1 if out was too small. */
int modtree_file_path(const modtree_dir_table_t *dirs, const modtree_file_table_t *files, 
                        int file_index, char *out, size_t out_size);

/* Method that returns how many files there are to parse and */
int numbres_of_files(const modtree_file_table_t* t);

#endif