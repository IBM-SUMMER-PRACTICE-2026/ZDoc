/*
 * doc_extractor's core orchestration: walk the project directory (via
 * module_tree), run the right parser per file (via child_parser.c), and
 * assemble the combined DxModel. dx_write.c handles emitting the result.
 */
#include "doc_extractor.h"
#include "child_parser.h"
#include "json_read.h"
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

    out->files = xmalloc(files.count ? files.count * sizeof(DxFile) : 1);
    for(size_t i = 0; i < files.count; i++) {
        DxFile *f = &out->files[i];
        memset(f, 0, sizeof *f);
        f->name = files.files[i].name ? xstrdup(files.files[i].name) : NULL;
        f->parent_dir_index = files.files[i].parent_dir_index;

        const LangEntry *lang = f->name ? lang_for_ext(f->name) : NULL;
        char full_path[2048];
        if(lang &&
           build_disk_path(&dirs, f->parent_dir_index, root_dir, f->name, full_path, sizeof full_path) == 0 &&
           run_parser(lang, parser_dir, full_path, f)) {
            if(!f->language) f->language = xstrdup(lang->language);
        } else {
            f->error = 1;
            if(lang) f->language = xstrdup(lang->language);
        }
        out->file_count++;
    }

    modtree_dir_table_free(&dirs);
    modtree_file_table_free(&files);
    return 1;
}

/* ---------------------------------------------------------------- dx_free */

static void free_symbol(DxSymbol *s) {
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
        for(size_t k = 0; k < f->symbol_count; k++) free_symbol(&f->symbols[k]);
        free(f->symbols);
    }
    free(m->files);

    memset(m, 0, sizeof *m);
}
