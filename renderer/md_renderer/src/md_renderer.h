/*
 * ZDoc md_renderer — renders the documentation model built by doc_extractor
 * as Markdown: one .md file per source module, plus a root index.md linking
 * to them.
 *
 * Takes doc_extractor's DxModel directly, in memory - no JSON, no
 * subprocess, no file format in between. See
 * extractor/doc_extractor/src/doc_extractor.h for the model itself
 * (DxModel/DxDir/DxFile/DxSymbol/DxParam); this header only adds the
 * rendering entry points.
 *
 * Part of the ZDoc renderer/ layer (see docs/ZDOC.md).
 */
#ifndef ZDOC_MD_RENDERER_H
#define ZDOC_MD_RENDERER_H

#include <stddef.h>

#include "../../../extractor/doc_extractor/src/doc_extractor.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Reconstruct a directory's path (e.g. "src/util") into 'out'. Returns 0 on
 * success, -1 if 'out' was too small. */
int md_dir_path(const DxModel *m, int dir_index, char *out, size_t out_size);

/* Reconstruct a file's path (e.g. "src/util/Helper.java") into 'out'.
 * Returns 0 on success, -1 if 'out' was too small. */
int md_file_path(const DxModel *m, size_t file_index, char *out, size_t out_size);

/* Render the whole model as Markdown under out_dir: one .md file per entry in
 * 'files' (mirroring the source directory structure) plus a root index.md
 * linking to each of them. 'title' (may be NULL) is used as index.md's
 * heading. Returns 0 on success, -1 on a write/IO failure. */
int md_render(const DxModel *m, const char *out_dir, const char *title);

#ifdef __cplusplus
}
#endif

#endif /* ZDOC_MD_RENDERER_H */
