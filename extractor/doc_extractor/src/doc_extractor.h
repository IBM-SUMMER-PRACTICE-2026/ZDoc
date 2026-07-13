/*
 * ZDoc doc_extractor — converts an already-parsed set of source files into
 * the combined documentation model (directory/file tree plus each file's
 * documented symbols).
 *
 * doc_extractor does no walking and no parsing of its own - that's done
 * elsewhere (the daemon: walks the tree via module_tree, runs the right
 * parser on every file, and produces one Module per successfully parsed
 * file). doc_extractor's job starts after that: given the already-built
 * module_tree tables and that array of parsed Module entries, assemble the
 * final DxModel. The result is a plain C struct, handed directly to
 * whichever renderer needs it (in the same process) - no JSON, no
 * serialization step.
 */
#ifndef ZDOC_DOC_EXTRACTOR_H
#define ZDOC_DOC_EXTRACTOR_H

#include <stddef.h>
#include <stdint.h>

#include "module_tree/modtree_tables.h"

/* The shape doc_extractor expects its already-parsed input in. This
 * mirrors parser/shared/parser_shared.h's Module/Symbol/InputParam - the
 * shared contract every parser now emits, populated by whatever walked the
 * tree and ran the parsers (the daemon) before calling into this code.
 * Defined locally here, rather than including that header directly, so
 * doc_extractor doesn't depend on any file under parser/ - only on this
 * documented shape staying in agreement with it. */
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

/* --------------------------------------------------------- output model */

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
    char     *diagram;    /* Mermaid flowchart source (AI Assisted mode), NULL when absent */
    char    **refs;       /* cross-referenced symbol names */
    size_t    ref_count;
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
    int       error;            /* 1 if no parsed module matched this file */
} DxFile;

typedef struct {
    DxDir  *dirs;
    size_t  dir_count;
    DxFile *files;
    size_t  file_count;
} DxModel;

/* Builds *out from an already-walked module tree (dirs/files) and an
 * already-parsed array of modules (modules[0..module_count)). Each module
 * is matched back to the file that produced it via pathIndex - the file
 * table index the caller (the daemon) stamped onto it while parsing - not
 * by any string comparison. A file with no matching module entry (parsing
 * failed, was skipped, or hasn't run yet) keeps DxFile.error = 1 and empty
 * symbols. language is derived from each file's own extension,
 * independently of whether a module matched, so it's always set. Always
 * returns 1. */
int dx_build_from_parsed(const modtree_dir_table_t *dirs, const modtree_file_table_t *files,
                          const Module *modules, size_t module_count, DxModel *out);

void dx_free(DxModel *m);

/* Frees a DxSymbol's string/array fields without freeing the struct itself
 * (it usually lives inside an array). */
void dx_free_symbol(DxSymbol *s);

#endif /* ZDOC_DOC_EXTRACTOR_H */
