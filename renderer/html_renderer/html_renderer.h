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

/* Module/Symbol/InputParam - the shared contract every parser emits -
 * come straight from parser_shared.h rather than being redefined here.
 * This header also pulls in modtree_tables.h transitively through it. */
#include "../../parser/shared/parser_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HTML_PATH_MAX 4096

/* Render the tree as out_dir/index.html plus one out_dir/<relpath>.html per
 * file (embedded CSS, no external dependencies). Each file is matched back
 * to its parsed module via pathIndex (modules[0..module_count)) - a file
 * with no match gets its own page with the "Parser failed for this file"
 * notice. 'title' (may be NULL) is used as index.html's heading; each
 * file's own page is headed with its filename. Returns ZDOC_OK on success;
 * ZDOC_PATH_TOO_LONG if a reconstructed output path overflowed its buffer,
 * or ZDOC_FILE_WRITE_FAILED if a page could not be opened, written or
 * closed. */
enum ZDoc_Error html_render(const modtree_dir_table_t *dirs, const modtree_file_table_t *files,
                 const Module *modules, size_t module_count,
                 const char *out_dir, const char *title);

#ifdef __cplusplus
}
#endif

#endif
