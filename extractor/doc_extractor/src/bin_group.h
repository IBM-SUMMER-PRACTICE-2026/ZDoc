/*
 * Groups discovered files by which parser binary handles them, and splits
 * each group into size-balanced ChunkWork entries.
 */
#ifndef ZDOC_BIN_GROUP_H
#define ZDOC_BIN_GROUP_H

#include <stddef.h>

#include "child_parser.h"
#include "doc_extractor.h"
#include "thread_pool.h"

/* All the file indices that share one parser binary. */
typedef struct {
    const char *parser_bin;
    size_t     *file_indices;
    size_t      count, cap;
} BinGroup;

/* Finds the group for parser_bin, or appends a new empty one. *groups /
 * *group_count / *group_cap are grown as needed. */
BinGroup *bin_group_find_or_create(BinGroup **groups, size_t *group_count,
                                    size_t *group_cap, const char *parser_bin);

void bin_group_add(BinGroup *g, size_t file_index);

/* Splits grp into ceil(grp->count / chunk_size) chunks and appends them to
 * work[], starting at *wi (which is advanced past the chunks just added).
 *
 * Files are sorted largest-first and dealt round-robin across the chunks -
 * like dealing cards - so no single thread ends up stuck with all the large
 * files while others in the same round sit idle. */
void bin_group_build_chunks(const BinGroup *grp, const LangEntry *const *file_lang,
                             const char *const *file_path, DxFile *out_files,
                             const char *parser_dir, size_t chunk_size,
                             ChunkWork *work, size_t *wi);

#endif /* ZDOC_BIN_GROUP_H */
