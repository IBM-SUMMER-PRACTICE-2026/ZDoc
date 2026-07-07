/*
  ZDoc documentation model - the normalised documentation-model JSON
  (parser output + module tree, combined by doc_extractor) parsed into an
  in-memory model that renderers walk.
  Part of the ZDoc renderer/ layer (see docs/ZDOC.md).
 */
#ifndef ZDOC_HTML_MODEL_H
#define ZDOC_HTML_MODEL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char *name;
    char *desc;
} HtmlParam;

typedef struct {
    char     *kind;
    uint32_t  line;
    char     *name;
    char     *signature;
    char     *brief;
    HtmlParam  *params;
    size_t    param_count;
    char     *returns;
    char     *notes;
} HtmlSymbol;

typedef struct {
    char     *name;         // directory name only, not a full path 
    int       parent_index; // index into HtmlModel.dirs, -1 for the root 
} HtmlDir;

typedef struct {
    char     *name;             // file name only, not a full path
    int       parent_dir_index; // index into HtmlModel.dirs
    char     *language;
    HtmlSymbol *symbols;
    size_t    symbol_count;
    int       error;            // 1 if the extractor couldn't run this file's parser
} HtmlFile;

typedef struct {
    HtmlDir  *dirs;
    size_t  dir_count;
    HtmlFile *files;
    size_t  file_count;
} HtmlModel;

/* Parse the documentation-model JSON in 'json' (length 'len') into 'out'.
 Returns 1 on success (out is populated, caller must html_model_free it),
 0 on a malformed document (out is left zeroed, nothing to free). 
 */
int html_model_parse(const char *json, size_t len, HtmlModel *out);

void html_model_free(HtmlModel *m);

#ifdef __cplusplus
}
#endif

#endif /* ZDOC_HTML_MODEL_H */
