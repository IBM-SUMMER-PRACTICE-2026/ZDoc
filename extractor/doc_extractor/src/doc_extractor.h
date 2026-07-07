/*
 * ZDoc doc_extractor — walks a project directory, invokes the right
 * language parser on every source file it finds, and combines the results
 * into one documentation model: the directory/file tree plus each file's
 * documented symbols.
 */
#ifndef ZDOC_DOC_EXTRACTOR_H
#define ZDOC_DOC_EXTRACTOR_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    char *name;
    char *desc;
} DxParam;

typedef struct {
    char     *kind;
    uint32_t  line;
    char     *name;
    char     *signature;
    char     *brief;
    DxParam  *params;
    size_t    param_count;
    char     *returns;
    char     *notes;
} DxSymbol;

typedef struct {
    char *name;         /* directory name only, not a full path */
    int   parent_index; /* index into DxModel.dirs, -1 for the root */
} DxDir;

typedef struct {
    char     *name;             /* file name only, not a full path */
    int       parent_dir_index; /* index into DxModel.dirs */
    char     *language;
    DxSymbol *symbols;
    size_t    symbol_count;
    int       error;            /* 1 if this file's parser couldn't be run or failed */
} DxFile;

typedef struct {
    DxDir  *dirs;
    size_t  dir_count;
    DxFile *files;
    size_t  file_count;
} DxModel;

/* Walks root_dir, runs the appropriate parser binary on every recognized
 * source file, and fills *out with the combined result. parser_dir, if
 * non-NULL, is searched for parser binaries instead of relying on PATH.
 * A single file's parser failing does not fail the whole build - that
 * file's DxFile.error is set to 1 instead. Returns 1 on success, 0 if
 * root_dir itself could not be walked at all. */
int dx_build(const char *root_dir, const char *parser_dir, DxModel *out);

void dx_free(DxModel *m);

/* Writes *m to o as the combined documentation-model JSON. */
void dx_write(const DxModel *m, FILE *o);

#endif /* ZDOC_DOC_EXTRACTOR_H */
