/*
 * doc_extractor's core orchestration: walk the project directory (via
 * module_tree), group files by which parser binary handles them, run each
 * group's parser in chunks (via child_parser.c) instead of once per file,
 * and assemble the combined DxModel. dx_write.c handles emitting the result.
 */
#include "doc_extractor.h"
#include "child_parser.h"
#include "json_read.h"
#include "module_tree/fs_walk.h"
#include "module_tree/modtree_tables.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

/* Chunk size is chosen per language group, scaled to the machine's CPU
 * count instead of a fixed guess - large enough to meaningfully amortize
 * process-spawn overhead, but small enough to leave several independent
 * chunks per core, since a future thread pool needs real units of work to
 * parallelize across, not one giant invocation per language. Clamped at
 * both ends: MIN_CHUNK_SIZE keeps tiny language groups from being split
 * into pointlessly small pieces, MAX_CHUNK_SIZE bounds how large a single
 * invocation (and its failure blast radius / argv length) can get. */
#define MIN_CHUNK_SIZE 8
#define MAX_CHUNK_SIZE 200
#define TARGET_CHUNKS_PER_CORE 4

static size_t detect_cpu_count(void) {
#ifdef _WIN32
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwNumberOfProcessors > 0 ? (size_t)info.dwNumberOfProcessors : 1;
#else
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (size_t)n : 1;
#endif
}

/* total_files is the size of ONE language group, not the whole project -
 * a project with 1000 Java files and 5 PLX files should chunk each of
 * those groups independently, not share one global chunk size. */
static size_t compute_chunk_size(size_t total_files) {
    size_t cores = detect_cpu_count();
    size_t target_chunks = cores * TARGET_CHUNKS_PER_CORE;
    size_t size = total_files / (target_chunks ? target_chunks : 1);
    if(size < MIN_CHUNK_SIZE) size = MIN_CHUNK_SIZE;
    if(size > MAX_CHUNK_SIZE) size = MAX_CHUNK_SIZE;
    return size;
}

/* strdup isn't ISO C (it's POSIX) and -std=c11 can hide its declaration
 * depending on the platform's libc, so use our own instead of relying on it. */
static char *xstrdup(const char *s) {
    size_t n = strlen(s);
    char *p = xmalloc(n + 1);
    memcpy(p, s, n + 1);
    return p;
}

/* modtree_file_path() reconstructs a path that already includes the seeded
 * root directory's own name (fs_walk seeds it before walking) - so root_dir
 * itself must NOT be prepended to that, or the root's name ends up
 * duplicated (e.g. "tests/fixture_project/fixture_project/..."). This walks
 * the same parent_index chain directly, stopping before the root entry
 * (parent_index == -1), and joins onto root_dir instead - since root_dir is
 * already that location on disk. */
static int build_disk_path(const modtree_dir_table_t *dirs, int dir_index,
                            const char *root_dir, const char *filename,
                            char *out, size_t out_size) {
    int chain[256];
    int depth = 0;
    int idx = dir_index;
    while(idx >= 0 && depth < (int)(sizeof chain / sizeof *chain)) {
        chain[depth++] = idx;
        idx = dirs->dirs[idx].parent_index;
    }

    int n = snprintf(out, out_size, "%s", root_dir);
    if(n < 0 || (size_t)n >= out_size) return -1;
    size_t used = (size_t)n;

    /* chain[depth-1] is the seeded root itself - skip it, only join the
     * segments strictly between root_dir and the file. */
    for(int k = depth - 2; k >= 0; k--) {
        const char *name = dirs->dirs[chain[k]].name ? dirs->dirs[chain[k]].name : "";
        size_t need = used + 1 + strlen(name);
        if(need >= out_size) return -1;
        out[used++] = '/';
        memcpy(out + used, name, strlen(name));
        used += strlen(name);
    }

    size_t need = used + 1 + strlen(filename);
    if(need >= out_size) return -1;
    out[used++] = '/';
    memcpy(out + used, filename, strlen(filename));
    used += strlen(filename);
    out[used] = '\0';
    return 0;
}

/* ------------------------------------------------------------ bin groups */

/* All the files that share one parser binary - built up in one pass over
 * every discovered file, then each processed in compute_chunk_size()-sized
 * batches. */
typedef struct {
    const char *parser_bin;
    size_t *file_indices;
    size_t count, cap;
} BinGroup;

static BinGroup *find_or_create_group(BinGroup **groups, size_t *group_count,
                                       size_t *group_cap, const char *parser_bin) {
    for(size_t i = 0; i < *group_count; i++)
        if(strcmp((*groups)[i].parser_bin, parser_bin) == 0) return &(*groups)[i];

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

static void group_add(BinGroup *g, size_t file_index) {
    if(g->count == g->cap) {
        g->cap = g->cap ? g->cap * 2 : 8;
        g->file_indices = xrealloc(g->file_indices, g->cap * sizeof(size_t));
    }
    g->file_indices[g->count++] = file_index;
}

/* --------------------------------------------------------------- dx_build */

int dx_build(const char *root_dir, const char *parser_dir, DxModel *out) {
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
            char full_path[2048];
            if(build_disk_path(&dirs, f->parent_dir_index, root_dir, f->name, full_path, sizeof full_path) == 0) {
                file_path[i] = xstrdup(full_path);
                BinGroup *g = find_or_create_group(&groups, &group_count, &group_cap, file_lang[i]->parser_bin);
                group_add(g, i);
            }
        }
    }

    modtree_dir_table_free(&dirs);
    modtree_file_table_free(&files);

    /* Pass 2: run each group's parser in chunks - one invocation per chunk
     * instead of one per file, amortizing process-spawn overhead. */
    for(size_t g = 0; g < group_count; g++) {
        BinGroup *grp = &groups[g];
        size_t chunk_size = compute_chunk_size(grp->count);
        for(size_t start = 0; start < grp->count; start += chunk_size) {
            size_t n = grp->count - start < chunk_size ? grp->count - start : chunk_size;

            const char **paths = xmalloc(n * sizeof(char *));
            DxFile **targets = xmalloc(n * sizeof(DxFile *));
            const LangEntry *lang = NULL;
            for(size_t k = 0; k < n; k++) {
                size_t fi = grp->file_indices[start + k];
                paths[k] = file_path[fi];
                targets[k] = &out->files[fi];
                if(!lang) lang = file_lang[fi];
            }

            run_parser_batch(lang, parser_dir, paths, targets, n);

            /* Anything still in error (whole invocation failed, or this
             * particular file's module was missing from the reply) keeps
             * f->error = 1 from pass 1 - just fill in a fallback language. */
            for(size_t k = 0; k < n; k++) {
                size_t fi = grp->file_indices[start + k];
                if(out->files[fi].error && !out->files[fi].language)
                    out->files[fi].language = xstrdup(file_lang[fi]->language);
            }

            free(paths);
            free(targets);
        }
        free(grp->file_indices);
    }
    free(groups);

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
