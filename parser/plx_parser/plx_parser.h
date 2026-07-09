#ifndef PLX_PARSER_H
#define PLX_PARSER_H

#include "helpers.h"
#include "../shared/parser_shared.h"

#define MAX_LINE 1024

/* ------------------------------------------------------------------ */
/* Label lookup                                                        */
/* ------------------------------------------------------------------ */

typedef enum {
    FIELD_NONE = -1, /* not a labeled line */
    FIELD_NAME,
    FIELD_DESCRIPTION,
    FIELD_INPUT,
    FIELD_OUTPUT,
    FIELD_UNKNOWN /* labeled line, but the label is not recognized */
} FieldId;

typedef struct {
    int active;      /* at least one recognized field collected */
    int closed;      /* end banner seen; awaiting the PROC statement */
    int prolog;      /* collected from a .plxmac Method Prolog block */
    FieldId current; /* field that continuation lines append to */
    int startLine;
    StrBuf name, description, output;
    StrList inputLines; /* input kept per line for param splitting */
} DocBlock;

/* Signature capture state: scan until ';' at paren depth 0, outside
 * comments and quoted strings. Comments are dropped from the signature. */
typedef struct {
    int depth;
    int inComment;
    int inString;
} SigState;

/* Symbol / Module output structs are the shared ones from parser_shared.h. */

#define PLX_COMMENT_LINE_WIDTH 71

Module *plx_parse_file(const char *path);
void plx_print_module(const Module *mod);
void plx_free_module(Module *mod);

#endif
