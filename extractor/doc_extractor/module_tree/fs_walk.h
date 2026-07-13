#ifndef ZDOC_FS_WALK_H
#define ZDOC_FS_WALK_H

#include "modtree_tables.h"
#include <stddef.h>

/* Maximum length (bytes) of the root prefix below. */
#define FS_WALK_PATH_MAX 4096

/* Absolute path of the walked root's PARENT directory, set by fs_walk().
 * Reconstructed file paths (see modtree_file_path) start with the root's own
 * name, so gluing this prefix on the front yields an absolute path that opens
 * from any working directory. Empty until fs_walk() has run. */
extern char fs_walk_root_prefix[FS_WALK_PATH_MAX];

/* Walks root_disk_path, interning every subdirectory into dirs and every
 * file whose extension matches one of `extensions` into files. Seeds the
 * root directory itself (its own name, parent = -1) before walking its
 * contents, so the caller does not need to intern it separately - this
 * happens regardless of `recursive`.
 *
 * extensions: array of extension strings, each including the leading dot
 *             (e.g. {".plx", ".pli"}). Comparison is case-sensitive.
 * extension_count: number of entries in extensions. Pass 0 (extensions may
 *                   then be NULL) to match every file regardless of extension.
 *
 * excludes: glob patterns (e.g. "*.test.c" or a bare directory name like
 *           "test") checked against both the bare entry name and the
 *           accumulated disk path; `*` matches any run of characters
 *           (including none, including path separators) and `?` matches
 *           exactly one character - a simple shell-style match, not a full
 *           gitignore-style segment-aware glob. A match excludes the entry
 *           entirely: a matching directory is neither interned nor
 *           descended into, a matching file is not interned.
 * exclude_count: number of entries in excludes. Pass 0 (excludes may then
 *                be NULL) to exclude nothing.
 *
 * recursive: when 0, subdirectories of root_disk_path are neither interned
 *            nor descended into - only root_disk_path's immediate file
 *            entries are considered. When non-zero, the walk descends into
 *            every non-excluded subdirectory as before.
 *
 * Returns 0 on success, -1 if root_disk_path could not be opened for
 * reading or an internal allocation failed.
 */
int fs_walk(const char* root_disk_path,
            modtree_dir_table_t* dirs,
            modtree_file_table_t* files,
            const char** extensions,
            size_t extension_count,
            const char** excludes,
            size_t exclude_count,
            int recursive);

#endif