#ifndef PLX_PARSER_H
#define PLX_PARSER_H

#define MAX_LINE 1024

typedef struct {
    char *name;
    char *description;
} InputParam;

typedef struct {
    int active;      /* at least one recognized field collected */
    int closed;      /* end banner seen; awaiting the PROC statement */
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

typedef struct {
    char *name;        /* normalized from the Title/Routine/... doc field */
    char *description;
    char *signature;
    InputParam *input;
    int inputCount;
    char *output;
} Symbol;

typedef struct {
    char *filename;
    Symbol *symbols;
    int symbolCount;
} Module;

#define PLX_COMMENT_LINE_WIDTH 71

Module *plx_parse_file(const char *path);
void plx_print_module(const Module *mod);
void plx_free_module(Module *mod);

#endif
