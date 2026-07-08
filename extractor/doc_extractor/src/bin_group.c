#include "bin_group.h"
#include "json_read.h" /* xmalloc / xrealloc */

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

BinGroup *bin_group_find_or_create(BinGroup **groups, size_t *group_count, size_t *group_cap, const char *parser_bin) {
    for(size_t i = 0; i < *group_count; i++) if(strcmp((*groups)[i].parser_bin, parser_bin) == 0) return &(*groups)[i];

    if(*group_count == *group_cap) {
        *group_cap = *group_cap ? *group_cap * 2 : 4;
        *groups = xrealloc(*groups, *group_cap * sizeof(BinGroup));
    }
    BinGroup *g = &(*groups)[(*group_count)++];
    g->parser_bin = parser_bin;
    g->file_indices = NULL;
    g->count = 0;
    g->cap = 0;
    return g;
}

void bin_group_add(BinGroup *g, size_t file_index) {
    if(g->count == g->cap) {
        g->cap = g->cap ? g->cap * 2 : 8;
        g->file_indices = xrealloc(g->file_indices, g->cap * sizeof(size_t));
    }
    g->file_indices[g->count++] = file_index;
}

/* ------------------------------------------------- size-aware balancing */

/* For sorting a language group's files by byte size (largest first) so
 * the round-robin interleave spreads heavy files across chunks evenly. */
typedef struct { size_t file_index; size_t file_size; } SizedFile;

static int cmp_sized_desc(const void *a, const void *b) {
    size_t sa = ((const SizedFile *)a)->file_size;
    size_t sb = ((const SizedFile *)b)->file_size;
    return (sa < sb) - (sa > sb); /* descending */
}

static size_t get_file_size(const char *path) {
    struct stat st;
    if(!path || stat(path, &st) != 0) return 0;
    return (size_t)st.st_size;
}

void bin_group_build_chunks(const BinGroup *grp, const LangEntry *const *file_lang,
                             const char *const *file_path, DxFile *out_files,
                             const char *parser_dir, size_t chunk_size,
                             ChunkWork *work, size_t *wi) {
    size_t n_chunks = (grp->count + chunk_size - 1) / chunk_size;

    /* Sort files by size, largest first. */
    SizedFile *sf = xmalloc(grp->count * sizeof(SizedFile));
    for(size_t k = 0; k < grp->count; k++) {
        size_t fi = grp->file_indices[k];
        sf[k].file_index = fi;
        sf[k].file_size  = get_file_size(file_path[fi]);
    }
    qsort(sf, grp->count, sizeof(SizedFile), cmp_sized_desc);

    /* Build each chunk by round-robin from the sorted list. */
    for(size_t c = 0; c < n_chunks; c++) {
        /* Count how many files land in this chunk. */
        size_t n = 0;
        for(size_t k = c; k < grp->count; k += n_chunks) n++;

        const char **paths = xmalloc(n * sizeof(char *));
        DxFile      **targets = xmalloc(n * sizeof(DxFile *));
        const LangEntry *lang = file_lang[sf[0].file_index];
        size_t idx = 0;
        for(size_t k = c; k < grp->count; k += n_chunks) {
            size_t fi = sf[k].file_index;
            paths[idx]   = file_path[fi];
            targets[idx] = &out_files[fi];
            idx++;
        }

        ChunkWork *w = &work[(*wi)++];
        w->lang       = lang;
        w->parser_dir = parser_dir;
        w->paths      = paths;
        w->targets    = targets;
        w->count      = n;
    }
    free(sf);
}
