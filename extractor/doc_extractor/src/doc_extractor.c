/*
 * doc_extractor's core logic: convert an already-walked module tree plus an
 * already-parsed array of modules into the combined DxModel. No walking, no
 * parsing, no calling into any parser - see doc_extractor.h.
 */
#include "doc_extractor.h"
#include "xalloc.h"

#include <stdlib.h>
#include <string.h>

/* Language is derived from the file's own extension, independently of
 * whether a parsed module matched it - a tiny local table, not tied to any
 * parser binary or source. */
typedef struct { const char *ext; const char *language; } LangEntry;

static const LangEntry LANGUAGES[] = {
    { ".java", "java" },
    { ".c",    "c" },
    { ".h",    "c" },
    { ".cpp",  "cpp" },
    { ".cxx",  "cpp" },
    { ".cc",   "cpp" },
    { ".hpp",  "cpp" },
    { ".plx",  "plx" },
    { ".pls",  "plx" },
    { ".plas", "plas" },
};
#define LANGUAGE_COUNT (sizeof LANGUAGES / sizeof *LANGUAGES)

static const char *language_for_name(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if(!dot) return NULL;
    for(size_t i = 0; i < LANGUAGE_COUNT; i++)
        if(strcmp(dot, LANGUAGES[i].ext) == 0) return LANGUAGES[i].language;
    return NULL;
}

/* strdup isn't ISO C (it's POSIX) and -std=c11 can hide its declaration
 * depending on the platform's libc, so use our own instead of relying on it. */
static char *dx_strdup(const char *s) {
    if(!s) return NULL;
    size_t n = strlen(s);
    char *p = xmalloc(n + 1);
    memcpy(p, s, n + 1);
    return p;
}

static void convert_symbol(const Symbol *src, DxSymbol *dst) {
    memset(dst, 0, sizeof *dst);
    dst->kind      = dx_strdup(src->type);
    dst->line      = src->line;
    dst->name      = dx_strdup(src->name);
    dst->signature = dx_strdup(src->signature);
    dst->brief     = dx_strdup(src->description);
    dst->returns   = dx_strdup(src->output);
    dst->notes     = dx_strdup(src->notes);
    dst->diagram   = dx_strdup(src->diagram);
    /* refs/ref_count: no upstream source yet (no parser or extraction step
     * computes cross-references) - stay NULL/0, same as an absent key would
     * have been under the old JSON contract. */

    if(src->inputCount > 0) {
        dst->params = xmalloc((size_t)src->inputCount * sizeof(DxParam));
        dst->param_count = (size_t)src->inputCount;
        for(int i = 0; i < src->inputCount; i++) {
            dst->params[i].name = dx_strdup(src->input[i].name);
            dst->params[i].desc = dx_strdup(src->input[i].description);
        }
    }
}

/* Finds the already-parsed module whose filename matches path, or NULL if
 * no module in the array corresponds to it. */
static const Module *find_module(const Module *modules, size_t module_count, const char *path) {
    for(size_t i = 0; i < module_count; i++)
        if(modules[i].filename && strcmp(modules[i].filename, path) == 0) return &modules[i];
    return NULL;
}

int dx_build_from_parsed(const modtree_dir_table_t *dirs, const modtree_file_table_t *files,
                          const Module *modules, size_t module_count, DxModel *out) {
    memset(out, 0, sizeof *out);

    out->dirs = xmalloc(dirs->count ? dirs->count * sizeof(DxDir) : 1);
    for(size_t i = 0; i < dirs->count; i++) {
        out->dirs[i].name = dx_strdup(dirs->dirs[i].name);
        out->dirs[i].parent_index = dirs->dirs[i].parent_index;
    }
    out->dir_count = dirs->count;

    out->file_count = files->count;
    out->files = xmalloc(files->count ? files->count * sizeof(DxFile) : 1);

    for(size_t i = 0; i < files->count; i++) {
        DxFile *f = &out->files[i];
        memset(f, 0, sizeof *f);
        f->name = dx_strdup(files->files[i].name);
        f->parent_dir_index = files->files[i].parent_dir_index;
        f->error = 1; /* pessimistic default, cleared only on a successful match below */

        const char *lang = f->name ? language_for_name(f->name) : NULL;
        if(!lang) continue;
        f->language = dx_strdup(lang);

        char path[2048];
        if(modtree_file_path(dirs, files, (int)i, path, sizeof path) != 0) continue;

        const Module *mod = find_module(modules, module_count, path);
        if(!mod) continue; /* no parsed module for this file - stays error = 1 */

        DxSymbol *symbols = mod->symbolCount
            ? xmalloc((size_t)mod->symbolCount * sizeof(DxSymbol)) : NULL;
        for(int k = 0; k < mod->symbolCount; k++)
            convert_symbol(&mod->symbols[k], &symbols[k]);

        f->symbols = symbols;
        f->symbol_count = (size_t)mod->symbolCount;
        f->error = 0;
    }

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
    free(s->diagram);
    for(size_t k = 0; k < s->param_count; k++) {
        free(s->params[k].name);
        free(s->params[k].desc);
    }
    free(s->params);
    for(size_t k = 0; k < s->ref_count; k++) free(s->refs[k]);
    free(s->refs);
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
