/*
 ZDoc html_renderer — renders the daemon's already-parsed output as one
 self-contained HTML page per module (mirroring the source directory
 structure), plus a root index.html linking to all of them, with
 per-symbol documentation sections on each page.

 Takes the module_tree tables and the parsed Module array directly, in
 memory - no JSON, no subprocess, no intermediate model in between. There
 used to be a doc_extractor stage that copied this same data into its own
 DxModel shape before handing it to the renderers; that copy added no real
 value once every parser already emits this same shared Module/Symbol
 shape, so this renderer now reads it directly and does its own per-file
 language lookup and module matching (mirrored in md_renderer, since both
 renderers need the same small amount of it).

 One real capability was dropped along with doc_extractor: cross-reference
 links (previously DxSymbol.refs). Nothing upstream ever actually populated
 that field - no parser or extraction step computes cross-references - so
 it was always empty in practice; dropping it loses nothing that worked.

 Part of the ZDoc renderer/ layer (see docs/ZDOC.md).
 */
#ifndef ZDOC_HTML_RENDERER_H
#define ZDOC_HTML_RENDERER_H

#include <stdint.h>

#include "../../extractor/doc_extractor/module_tree/modtree_tables.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The shape html_renderer expects its already-parsed input in. This
 * mirrors parser/shared/parser_shared.h's Module/Symbol/InputParam - the
 * shared contract every parser emits - defined locally here rather than
 * including that header directly, so this renderer doesn't depend on any
 * file under parser/, only on this documented shape staying in agreement
 * with it. */
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
    char       *diagram; /* Mermaid flowchart source (AI Assisted mode), NULL when absent */
} Symbol;

typedef struct {
    char   *filename;
    Symbol *symbols;
    int     symbolCount;
    int     symbolCap;
    int     pathIndex; /* index into the file table this module was parsed from */
} Module;

/* Render the tree as out_dir/index.html plus one out_dir/<relpath>.html per
 * file (embedded CSS, no external dependencies). Each file is matched back
 * to its parsed module via pathIndex (modules[0..module_count)) - a file
 * with no match gets its own page with the "Parser failed for this file"
 * notice. 'title' (may be NULL) is used as index.html's heading; each
 * file's own page is headed with its filename. Returns 0 on success, -1 on
 * a write/IO failure. */
int html_render(const modtree_dir_table_t *dirs, const modtree_file_table_t *files,
                 const Module *modules, size_t module_count,
                 const char *out_dir, const char *title);

#ifdef __cplusplus
}
#endif

#endif
