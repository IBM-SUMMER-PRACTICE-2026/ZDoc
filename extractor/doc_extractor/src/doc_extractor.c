/*
 * doc_extractor's core orchestration: walk the project directory (via
 * module_tree), group files by which parser binary handles them (bin_group.c),
 * resolve each file's disk path (path_build.c), run each group's parser in
 * chunks across a thread pool (thread_pool.c) instead of once per file, and
 * assemble the combined DxModel. dx_write.c handles emitting the result.
 */
#include "doc_extractor.h"
#include "child_parser.h"
#include "json_read.h"
#include "bin_group.h"
#include "path_build.h"
#include "thread_pool.h"
#include "module_tree/fs_walk.h"
#include "module_tree/modtree_tables.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* strdup isn't ISO C (it's POSIX) and -std=c11 can hide its declaration
 * depending on the platform's libc, so use our own instead of relying on it. */
static char *xstrdup(const char *s) {
    size_t n = strlen(s);
    char *p = xmalloc(n + 1);
    memcpy(p, s, n + 1);
    return p;
}

/* --------------------------------------------------------------- dx_build */

int dx_build(const char *root_dir, const char *parser_dir, DxModel *out, int print_stats) {
    memset(out, 0, sizeof *out);

    modtree_dir_table_t dirs;
    modtree_file_table_t files;
    modtree_dir_table_init(&dirs);
    modtree_file_table_init(&files);

    size_t lang_count = lang_table_count();
    const char *extensions[32];
    if(lang_count > sizeof extensions / sizeof *extensions)
        lang_count = sizeof extensions / sizeof *extensions;
    for(size_t i = 0; i < lang_count; i++) extensions[i] = lang_table_entry(i)->ext;

    if(fs_walk(root_dir, &dirs, &files, extensions, lang_count) != 0) {
        modtree_dir_table_free(&dirs);
        modtree_file_table_free(&files);
        return 0;
    }

    out->dirs = xmalloc(dirs.count ? dirs.count * sizeof(DxDir) : 1);
    for(size_t i = 0; i < dirs.count; i++) {
        out->dirs[i].name = dirs.dirs[i].name ? xstrdup(dirs.dirs[i].name) : NULL;
        out->dirs[i].parent_index = dirs.dirs[i].parent_index;
    }
    out->dir_count = dirs.count;

    out->file_count = files.count;
    out->files = xmalloc(files.count ? files.count * sizeof(DxFile) : 1);
    const LangEntry **file_lang = xmalloc(files.count ? files.count * sizeof(LangEntry *) : 1);
    char **file_path = xmalloc(files.count ? files.count * sizeof(char *) : 1);

    BinGroup *groups = NULL;
    size_t group_count = 0, group_cap = 0;

    /* Pass 1: fill in each file's own fields, resolve its disk path, and
     * group it by which parser binary should handle it - no parser is run
     * yet, this just prepares the batches. */
    for(size_t i = 0; i < files.count; i++) {
        DxFile *f = &out->files[i];
        memset(f, 0, sizeof *f);
        f->name = files.files[i].name ? xstrdup(files.files[i].name) : NULL;
        f->parent_dir_index = files.files[i].parent_dir_index;
        f->error = 1; /* pessimistic default, cleared only on a successful match below */

        file_lang[i] = f->name ? lang_for_ext(f->name) : NULL;
        file_path[i] = NULL;

        if(file_lang[i]) {
            /* language is known from the file's own extension - set it now
             * rather than waiting on the parser to echo it back, so it's
             * always present even if that file's parser invocation fails. */
            f->language = xstrdup(file_lang[i]->language);

            char full_path[2048];
            if(build_disk_path(&dirs, f->parent_dir_index, root_dir, f->name, full_path, sizeof full_path) == 0) {
                file_path[i] = xstrdup(full_path);
                BinGroup *g = bin_group_find_or_create(&groups, &group_count, &group_cap, file_lang[i]->parser_bin);
                bin_group_add(g, i);
            }
        }
    }

    modtree_dir_table_free(&dirs);
    modtree_file_table_free(&files);

    /* Pass 2: build all chunks across all groups into a flat work queue,
     * then run them all through the thread pool. Pre-building avoids
     * allocating inside threaded code. */

    /* 2a: count total chunks so we can allocate once. One chunk_size is
     * computed once here (targeting one chunk per core across the COMBINED
     * file count of every group) and reused for every group below - see
     * chunk_pool_compute_chunk_size(). */
    size_t total_matched_files = 0;
    for(size_t g = 0; g < group_count; g++) total_matched_files += groups[g].count;
    const size_t chunk_size = chunk_pool_compute_chunk_size(total_matched_files);

    size_t total_chunks = 0;
    for(size_t g = 0; g < group_count; g++)
        total_chunks += (groups[g].count + chunk_size - 1) / chunk_size;

    if(print_stats) {
        size_t cores = chunk_pool_cpu_count();
        fprintf(stderr, "zdoc-doc-extractor stats: %zu core%s detected, chunk size %zu, "
                        "%zu file%s across %zu language group%s -> %zu chunk%s "
                        "(%zu round%s through the pool)\n",
                cores, cores == 1 ? "" : "s", chunk_size,
                total_matched_files, total_matched_files == 1 ? "" : "s",
                group_count, group_count == 1 ? "" : "s",
                total_chunks, total_chunks == 1 ? "" : "s",
                (total_chunks + cores - 1) / (cores ? cores : 1),
                (total_chunks + cores - 1) / (cores ? cores : 1) == 1 ? "" : "s");
        for(size_t g = 0; g < group_count; g++) {
            size_t n_chunks = (groups[g].count + chunk_size - 1) / chunk_size;
            fprintf(stderr, "  %-24s %6zu file%s -> %3zu chunk%s\n",
                    groups[g].parser_bin, groups[g].count, groups[g].count == 1 ? "" : "s",
                    n_chunks, n_chunks == 1 ? "" : "s");
        }
    }

    ChunkWork *work = xmalloc(total_chunks ? total_chunks * sizeof(ChunkWork) : 1);
    size_t wi = 0;

    /* 2b: fill in each ChunkWork item, group by group. */
    for(size_t g = 0; g < group_count; g++)
        bin_group_build_chunks(&groups[g], file_lang, (const char *const *)file_path,
                                out->files, parser_dir, chunk_size, work, &wi);

    /* 2c: dispatch across the thread pool and wait for everything to finish. */
    chunk_pool_run(work, total_chunks);

    /* 2d: anything still marked error (whole invocation failed, or this
     * file's module was missing from the reply) keeps f->error = 1 from
     * pass 1 - language is already set from pass 1 regardless, so nothing
     * else to backfill here. */
    for(size_t g = 0; g < group_count; g++) free(groups[g].file_indices);
    free(groups);

    /* Free chunk work arrays. */
    for(size_t i = 0; i < total_chunks; i++) {
        free(work[i].paths);
        free(work[i].targets);
    }
    free(work);

    for(size_t i = 0; i < out->file_count; i++) free(file_path[i]);
    free(file_path);
    free((void *)file_lang);

    return 1;
}

/* ---------------------------------------------------------------- dx_free */

void dx_free_symbol(DxSymbol *s) {
    free(s->kind);
    free(s->name);
    free(s->signature);
    free(s->brief);
    free(s->returns);
    free(s->notes);
    for(size_t k = 0; k < s->param_count; k++) {
        free(s->params[k].name);
        free(s->params[k].desc);
    }
    free(s->params);
}

void dx_free(DxModel *m) {
    for(size_t i = 0; i < m->dir_count; i++) free(m->dirs[i].name);
    free(m->dirs);

    for(size_t i = 0; i < m->file_count; i++) {
        DxFile *f = &m->files[i];
        free(f->name);
        free(f->language);
        for(size_t k = 0; k < f->symbol_count; k++) dx_free_symbol(&f->symbols[k]);
        free(f->symbols);
    }
    free(m->files);

    memset(m, 0, sizeof *m);
}
