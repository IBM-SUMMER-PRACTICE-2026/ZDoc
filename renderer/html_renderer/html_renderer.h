/*
 ZDoc html_renderer — renders the normalised documentation-model JSON
 (parser output + module tree, combined by doc_extractor) as a single
 self-contained index.html: expandable <details> nodes per directory and
 module, with per-symbol documentation sections.
 Part of the ZDoc renderer/ layer (see docs/ZDOC.md).
 */
#ifndef ZDOC_HTML_RENDERER_H
#define ZDOC_HTML_RENDERER_H

#include "html_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Render the whole model as one self-contained out_dir/index.html (embedded
 CSS, no external dependencies). 'title' (may be NULL) is used as the page
 heading. Returns 0 on success, -1 on a write/IO failure. */
int html_render(const HtmlModel *m, const char *out_dir, const char *title);

#ifdef __cplusplus
}
#endif

#endif
