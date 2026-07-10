/*
 * ZDoc c_parser — fast C and C++ declaration/doc-comment parser.
 *
 * Single-pass scanner: no regex, no AST, no heap churn. Extracts
 * documented symbols (functions, prototypes, macros, types, variables)
 * together with their Doxygen-style doc comments.
 *
 * Part of the ZDoc parser/ layer (see docs/ZDOC.md).
 */
#ifndef ZDOC_C_PARSER_H
#define ZDOC_C_PARSER_H

#include <stddef.h>
#include <stdint.h>

#include "../shared/parser_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CP_SYM_FUNCTION,   /* function definition (has a body)          */
    CP_SYM_PROTOTYPE,  /* function declaration / class method decl  */
    CP_SYM_MACRO,      /* #define                                   */
    CP_SYM_TYPE,       /* struct/class/union/enum/typedef/using     */
    CP_SYM_VARIABLE    /* documented file- or class-scope variable  */
} cp_symbol_kind;

typedef struct cp_result {
    Symbol *syms;
    size_t n, cap;
    const char *err;
} cp_result;

/* Parse a buffer (copied internally; caller keeps ownership of src). */
cp_result *cp_parse_buffer(const char *src, size_t len);

/* Read and parse a file into the internal cp_result. Never returns NULL
 * except on allocation failure; check cp_error(). */
cp_result *cp_parser(const char *path);

/* Read and parse a file into the shared Module (parser_shared.h). Returns a
 * heap Module the caller releases with free_module(). On a parse/IO error,
 * prints to stderr and returns an empty Module (symbolCount 0). Returns NULL
 * only on allocation failure. */
struct Module *cp_parse_file(const char *path);

/* Symbols in source order. Valid until cp_result_free(). */
const Symbol *cp_symbols(const cp_result *r, size_t *count);

/* NULL on success, else a message describing the failure. */
const char *cp_error(const cp_result *r);

const char *cp_symbol_kind_name(cp_symbol_kind k);

void cp_result_free(cp_result *r);

#ifdef __cplusplus
}
#endif

#endif /* ZDOC_C_PARSER_H */
