/*
 * ZDoc md_renderer — renders the daemon's already-parsed output as
 * Markdown: one .md file per source module, plus a root index.md linking
 * to them.
 *
 * Takes the module_tree tables and the parsed Module array directly, in
 * memory - no JSON, no subprocess, no intermediate model in between. There
 * used to be a doc_extractor stage that copied this same data into its own
 * DxModel shape before handing it to the renderers; that copy added no
 * real value once every parser already emits this same shared Module/
 * Symbol shape, so this renderer now reads it directly and does its own
 * per-file language lookup and module matching (mirrored in
 * html_renderer, since both renderers need the same small amount of it).
 *
 * Part of the ZDoc renderer/ layer (see docs/ZDOC.md).
 */
#ifndef ZDOC_MD_RENDERER_H
#define ZDOC_MD_RENDERER_H

#include <stddef.h>
#include <stdint.h>

#include "../../../extractor/doc_extractor/module_tree/modtree_tables.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The shape md_renderer expects its already-parsed input in. This mirrors
 * parser/shared/parser_shared.h's Module/Symbol/InputParam - the shared
 * contract every parser emits - defined locally here rather than including
 * that header directly, so this renderer doesn't depend on any file under
 * parser/, only on this documented shape staying in agreement with it. */
typedef struct {
    char *name;
    char *description;
} InputParam;

typedef struct {
    char       *name;
    char       *description;
    char       *signature;
    InputParam *input;
    int         inputCount;
    char       *output;
    char       *notes;
    uint32_t    line;
    char       *type;
    char       *diagram;
} Symbol;

typedef struct {
    char   *filename;
    Symbol *symbols;
    int     symbolCount;
    int     symbolCap;
    int     pathIndex; /* index into the file table this module was parsed from */
} Module;

/* Render the whole tree as Markdown under out_dir: one .md file per entry
 * in 'files' (mirroring the source directory structure) plus a root
 * index.md linking to each of them. Each file is matched back to its
 * parsed module via pathIndex (modules[0..module_count)) - a file with no
 * match is rendered with zero symbols. 'title' (may be NULL) is used as
 * index.md's heading. Returns 0 on success, -1 on a write/IO failure. */
int md_render(const modtree_dir_table_t *dirs, const modtree_file_table_t *files,
              const Module *modules, size_t module_count,
              const char *out_dir, const char *title);

#ifdef __cplusplus
}
#endif

#endif /* ZDOC_MD_RENDERER_H */
