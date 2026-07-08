/*
 * ZDoc md_renderer — renders the normalised documentation-model JSON
 * (parser output + module tree, combined by doc_extractor) as Markdown:
 * one .md file per source module, plus a root index.md linking to them.
 *
 * Part of the ZDoc renderer/ layer (see docs/ZDOC.md).
 */
#ifndef ZDOC_MD_RENDERER_H
#define ZDOC_MD_RENDERER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char *name;
    char *desc;
} MdParam;

typedef struct {
    char     *kind;
    uint32_t  line;
    char     *name;
    char     *signature;
    char     *brief;
    MdParam  *params;
    size_t    param_count;
    char     *returns;
    char     *notes;
} MdSymbol;

typedef struct {
    char     *name;         /* directory name only, not a full path */
    int       parent_index; /* index into MdModel.dirs, -1 for the root */
} MdDir;

typedef struct {
    char     *name;             /* file name only, not a full path */
    int       parent_dir_index; /* index into MdModel.dirs */
    char     *language;
    MdSymbol *symbols;
    size_t    symbol_count;
} MdFile;

typedef struct {
    MdDir  *dirs;
    size_t  dir_count;
    MdFile *files;
    size_t  file_count;
} MdModel;

/* Parse the documentation-model JSON in 'json' (length 'len') into 'out'.
 * Returns 1 on success (out is populated, caller must md_model_free it),
 * 0 on a malformed document (out is left zeroed, nothing to free). */
int md_model_parse(const char *json, size_t len, MdModel *out);

void md_model_free(MdModel *m);

/* Reconstruct a directory's path (e.g. "src/util") into 'out'. Returns 0 on
 * success, -1 if 'out' was too small. */
int md_dir_path(const MdModel *m, int dir_index, char *out, size_t out_size);

/* Reconstruct a file's path (e.g. "src/util/Helper.java") into 'out'.
 * Returns 0 on success, -1 if 'out' was too small. */
int md_file_path(const MdModel *m, size_t file_index, char *out, size_t out_size);

/* Render the whole model as Markdown under out_dir: one .md file per entry in
 * 'files' (mirroring the source directory structure) plus a root index.md
 * linking to each of them. 'title' (may be NULL) is used as index.md's
 * heading. Returns 0 on success, -1 on a write/IO failure. */
int md_render(const MdModel *m, const char *out_dir, const char *title);

#ifdef __cplusplus
}
#endif

#endif /* ZDOC_MD_RENDERER_H */
