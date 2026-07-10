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

/* Parse a buffer into a heap Module (parser_shared.h); the caller releases it
 * with free_module(). The source is copied internally (caller keeps ownership
 * of src) and the module's filename is set to "<buffer>". On failure (out of
 * memory), prints a diagnostic to stderr and returns NULL. */
Module *cp_parse_buffer(const char *src, size_t len);

/* Read and parse a file into a heap Module (parser_shared.h); the caller
 * releases it with free_module(). On failure (I/O error or out of memory),
 * prints a diagnostic to stderr and returns NULL. */
Module *cp_parser(const char *path);

/* Like cp_parser(), but never returns NULL: on a parse/IO error (already
 * reported to stderr by cp_parser()) an empty Module (symbolCount 0) is
 * returned instead. */
Module *cp_parse_file(const char *path);

const char *cp_symbol_kind_name(cp_symbol_kind k);

#ifdef __cplusplus
}
#endif

#endif /* ZDOC_C_PARSER_H */
