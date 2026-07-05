#ifndef PLX_PARSER_H
#define PLX_PARSER_H

typedef struct {
    char *name;
    char *description;
} InputParam;

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
