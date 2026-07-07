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

typedef struct {
    const char *name;
    const char *desc;
} cp_doc_param;

typedef struct {
    const char   *brief;    /* NULL when absent */
    const char   *returns;  /* NULL when absent */
    const char   *notes;    /* NULL when absent */
    cp_doc_param *params;
    size_t        nparams;
} cp_doc;

typedef struct {
    cp_symbol_kind kind;
    const char    *name;
    const char    *signature; /* whitespace-collapsed, comment-free */
    uint32_t       line;      /* 1-based line of the declaration    */
    int            has_doc;
    cp_doc         doc;
    /* CP_OPT_AI_CONTEXT only (NULL/0 otherwise): */
    const char    *body;      /* verbatim source incl. signature+body */
    uint32_t       line_end;  /* 1-based last line of the body        */
} cp_symbol;

/* CP_OPT_AI_CONTEXT only: verbatim declaration for AI closure assembly
 * (see docs/zdoc-ai-mode.md). `names` lists the identifiers the parser
 * knows the declaration introduces; the AI layer additionally tokenizes
 * `text` to satisfy the every-name rule. */
typedef struct {
    const char **names;
    size_t       nnames;
    const char  *text;
    uint32_t     line;
} cp_declaration;

typedef struct cp_result cp_result;

/* Option flags. */
#define CP_OPT_AI_CONTEXT 1u /* capture bodies + declarations for AI mode */

/* Parse a buffer (copied internally; caller keeps ownership of src). */
cp_result *cp_parse_buffer(const char *src, size_t len);
cp_result *cp_parse_buffer_opts(const char *src, size_t len, unsigned opts);

/* Read and parse a file. Never returns NULL; check cp_error(). */
cp_result *cp_parse_file(const char *path);
cp_result *cp_parse_file_opts(const char *path, unsigned opts);

/* Declarations in source order (empty unless CP_OPT_AI_CONTEXT). */
const cp_declaration *cp_declarations(const cp_result *r, size_t *count);

/* Symbols in source order. Valid until cp_result_free(). */
const cp_symbol *cp_symbols(const cp_result *r, size_t *count);

/* Module-level doc block (a comment carrying @file/@mainpage), if any. */
int cp_module_doc(const cp_result *r, cp_doc *out);

/* NULL on success, else a message describing the failure. */
const char *cp_error(const cp_result *r);

const char *cp_symbol_kind_name(cp_symbol_kind k);

void cp_result_free(cp_result *r);

#ifdef __cplusplus
}
#endif

#endif /* ZDOC_C_PARSER_H */
