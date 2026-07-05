#ifndef ZDOC_FS_WALK_H
#define ZDOC_FS_WALK_H

#include "modtree_tables.h"
#include <stddef.h>

/* Recursively walks root_disk_path, interning every subdirectory into dirs
 * and every file whose extension matches one of `extensions` into files.
 * Seeds the root directory itself (its own name, parent = -1) before
 * walking its contents, so the caller does not need to intern it separately.
 *
 * extensions: array of extension strings, each including the leading dot
 *             (e.g. {".plx", ".pli"}). Comparison is case-sensitive.
 * extension_count: number of entries in extensions. Pass 0 (extensions may
 *                   then be NULL) to match every file regardless of extension.
 *
 * Returns 0 on success, -1 if root_disk_path could not be opened for
 * reading or an internal allocation failed.
 */
int fs_walk(const char* root_disk_path,
            modtree_dir_table_t* dirs,
            modtree_file_table_t* files,
            const char** extensions,
            size_t extension_count);

#endif