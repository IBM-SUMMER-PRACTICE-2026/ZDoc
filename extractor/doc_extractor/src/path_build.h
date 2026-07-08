/* Reconstructs a file's full path on disk from module_tree's directory table. */
#ifndef ZDOC_PATH_BUILD_H
#define ZDOC_PATH_BUILD_H

#include <stddef.h>

#include "module_tree/modtree_tables.h"

/* modtree_file_path() reconstructs a path that already includes the seeded
 * root directory's own name (fs_walk seeds it before walking) - so root_dir
 * itself must NOT be prepended to that, or the root's name ends up
 * duplicated (e.g. "tests/fixture_project/fixture_project/..."). This walks
 * the same parent_index chain directly, stopping before the root entry
 * (parent_index == -1), and joins onto root_dir instead - since root_dir is
 * already that location on disk. Returns 0 on success, -1 if out_size was
 * too small. */
int build_disk_path(const modtree_dir_table_t *dirs, int dir_index,
                     const char *root_dir, const char *filename,
                     char *out, size_t out_size);

#endif /* ZDOC_PATH_BUILD_H */
