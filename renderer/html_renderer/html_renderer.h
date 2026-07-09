/*
 ZDoc html_renderer — renders the documentation model built by doc_extractor
 as a single self-contained index.html: expandable <details> nodes per
 directory and module, with per-symbol documentation sections.

 Takes doc_extractor's DxModel directly, in memory - no JSON, no
 subprocess, no file format in between. See
 extractor/doc_extractor/src/doc_extractor.h for the model itself
 (DxModel/DxDir/DxFile/DxSymbol/DxParam - DxSymbol carries diagram/refs for
 AI Assisted mode); this header only adds the rendering entry point.

 Part of the ZDoc renderer/ layer (see docs/ZDOC.md).
 */
#ifndef ZDOC_HTML_RENDERER_H
#define ZDOC_HTML_RENDERER_H

#include "../../extractor/doc_extractor/src/doc_extractor.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Render the whole model as one self-contained out_dir/index.html (embedded
 CSS, no external dependencies). 'title' (may be NULL) is used as the page
 heading. Returns 0 on success, -1 on a write/IO failure. */
int html_render(const DxModel *m, const char *out_dir, const char *title);

#ifdef __cplusplus
}
#endif

#endif
