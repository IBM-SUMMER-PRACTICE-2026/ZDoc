/* Closure assembly: for one symbol, collect only the declarations its body
 * references, within a character budget, so each AI call gets enough context
 * to name things meaningfully without receiving the whole file.
 * See ai/bob_client/README.md §Context closure. */
#ifndef ZDOC_BC_CLOSURE_H
#define ZDOC_BC_CLOSURE_H

#include <stddef.h>

typedef enum {
    BC_LANG_PLX,
    BC_LANG_PLAS,
    BC_LANG_C,
    BC_LANG_CPP,
    BC_LANG_JAVA,
    BC_LANG_ASM,
    BC_LANG_PASCAL,
    BC_LANG_UNKNOWN
} bc_lang;

bc_lang bc_lang_parse(const char *s);       /* "c", "cpp", "plx", ... */
const char *bc_lang_display(bc_lang l);     /* "C", "C++", "PL/X", "HLASM"... */
int bc_lang_folds_case(bc_lang l);          /* 1 for PL/X, PLAS, HLASM, Pascal */

typedef struct {
    const char **names;  /* every identifier this declaration introduces */
    size_t nnames;
    const char *text;    /* verbatim source */
    long line;
} bc_decl;

/* Name -> declaration index. Also indexes each declaration under all
 * identifiers tokenized from its text (explicit names take precedence),
 * which implements the every-name rule even for under-reporting parsers. */
typedef struct bc_index bc_index;
bc_index *bc_index_build(const bc_decl *decls, size_t ndecls, bc_lang lang);
const bc_decl *bc_index_lookup(const bc_index *idx, const char *name);
void bc_index_free(bc_index *idx);

/* Sorted, unique identifiers appearing in `body`, minus the language's
 * keyword set. Free with bc_refs_free. Over-collection is intended. */
char **bc_extract_refs(const char *body, bc_lang lang, size_t *out_n);
void bc_refs_free(char **refs, size_t n);

/* Tiered, budgeted closure. Returns a malloc'd array of borrowed
 * bc_decl pointers in deterministic order; caller free()s the array. */
const bc_decl **bc_closure(const char *body, const bc_index *idx,
                           bc_lang lang, size_t max_chars,
                           int transitive_depth, size_t *out_n);

/* Snippet exactly as the zdoc-diagram skill expects (DOC / DECLARATIONS /
 * CALLEES / FUNCTION). Empty sections are omitted. malloc'd. */
char *bc_build_snippet(const char *doc_brief, const bc_decl **closure,
                       size_t nclosure, const char **callee_lines,
                       size_t ncallees, bc_lang lang, const char *body);

#endif
