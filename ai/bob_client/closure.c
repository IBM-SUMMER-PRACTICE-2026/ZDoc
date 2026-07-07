#include "closure.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

/* ------------------------------------------------------------- languages */

bc_lang bc_lang_parse(const char *s)
{
    if (!s)
        return BC_LANG_UNKNOWN;
    if (!strcmp(s, "plx") || !strcmp(s, "pls")) return BC_LANG_PLX;
    if (!strcmp(s, "plas")) return BC_LANG_PLAS;
    if (!strcmp(s, "c")) return BC_LANG_C;
    if (!strcmp(s, "cpp") || !strcmp(s, "c++") || !strcmp(s, "cxx"))
        return BC_LANG_CPP;
    if (!strcmp(s, "java")) return BC_LANG_JAVA;
    if (!strcmp(s, "asm") || !strcmp(s, "assembler") || !strcmp(s, "hlasm"))
        return BC_LANG_ASM;
    if (!strcmp(s, "pascal") || !strcmp(s, "pas")) return BC_LANG_PASCAL;
    return BC_LANG_UNKNOWN;
}

const char *bc_lang_display(bc_lang l)
{
    switch (l) {
    case BC_LANG_PLX: return "PL/X";
    case BC_LANG_PLAS: return "PLAS";
    case BC_LANG_C: return "C";
    case BC_LANG_CPP: return "C++";
    case BC_LANG_JAVA: return "Java";
    case BC_LANG_ASM: return "HLASM";
    case BC_LANG_PASCAL: return "Pascal";
    default: return "C";
    }
}

int bc_lang_folds_case(bc_lang l)
{
    return l == BC_LANG_PLX || l == BC_LANG_PLAS || l == BC_LANG_ASM ||
           l == BC_LANG_PASCAL;
}

/* Per-language keyword/builtin tables, uppercase for case-folding langs.
 * Small on purpose: precision comes from the index lookup, not from here. */
static const char *const kw_c[] = {
    "auto", "break", "case", "char", "const", "continue", "default", "do",
    "double", "else", "enum", "extern", "float", "for", "goto", "if",
    "inline", "int", "long", "register", "restrict", "return", "short",
    "signed", "sizeof", "static", "struct", "switch", "typedef", "union",
    "unsigned", "void", "volatile", "while", "NULL", "size_t", "bool",
    "true", "false", "uint32_t", "int32_t", "uint64_t", "int64_t",
    "uint16_t", "int16_t", "uint8_t", "int8_t",
};
static const char *const kw_cpp[] = {
    "class", "namespace", "template", "typename", "new", "delete", "this",
    "public", "private", "protected", "virtual", "override", "final",
    "using", "nullptr", "constexpr", "noexcept", "operator", "friend",
    "mutable", "explicit", "try", "catch", "throw", "std",
};
static const char *const kw_java[] = {
    "abstract", "assert", "boolean", "break", "byte", "case", "catch",
    "char", "class", "continue", "default", "do", "double", "else", "enum",
    "extends", "final", "finally", "float", "for", "if", "implements",
    "import", "instanceof", "int", "interface", "long", "native", "new",
    "package", "private", "protected", "public", "return", "short",
    "static", "super", "switch", "synchronized", "this", "throw", "throws",
    "try", "void", "volatile", "while", "true", "false", "null", "String",
};
static const char *const kw_plx[] = {
    "PROC", "END", "RETURN", "RETURNS", "IF", "THEN", "ELSE", "DO", "WHILE",
    "UNTIL", "DCL", "DECLARE", "BASED", "PTR", "POINTER", "FIXED", "BIN",
    "BIT", "CHAR", "CONSTANT", "INIT", "ENTRY", "CALL", "SELECT", "WHEN",
    "OTHERWISE", "LEAVE", "ITERATE", "TO", "BY", "GOTO", "LENGTH", "ADDR",
    "NULL", "ON", "SIGNAL", "STATIC", "AUTOMATIC", "BUILTIN", "OBTAIN",
    "RELEASE", "GENERATE", "RESPECIFY", "ANS", "VALUE",
};
static const char *const kw_asm[] = {
    "DS", "DC", "EQU", "CSECT", "DSECT", "USING", "DROP", "STM", "LM", "LR",
    "LA", "ST", "L", "B", "BR", "BNZ", "BZ", "BE", "BNE", "BH", "BL", "BNH",
    "BNL", "LTR", "XC", "MVC", "CLC", "CLI", "MVI", "GETMAIN", "FREEMAIN",
    "SAVE", "RETURN", "END", "START", "AMODE", "RMODE", "EJECT", "SPACE",
    "TITLE", "AR", "SR", "MR", "DR", "C", "CR", "A", "S", "IC", "STC",
    "BCT", "BCTR", "SLL", "SRL", "NR", "OR", "XR", "BAL", "BALR", "BAS",
    "BASR", "R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7", "R8", "R9",
    "R10", "R11", "R12", "R13", "R14", "R15",
};
static const char *const kw_pascal[] = {
    "AND", "ARRAY", "BEGIN", "CASE", "CONST", "DIV", "DO", "DOWNTO", "ELSE",
    "END", "FILE", "FOR", "FUNCTION", "GOTO", "IF", "IN", "LABEL", "MOD",
    "NIL", "NOT", "OF", "OR", "PACKED", "PROCEDURE", "PROGRAM", "RECORD",
    "REPEAT", "SET", "THEN", "TO", "TYPE", "UNIT", "UNTIL", "USES", "VAR",
    "WHILE", "WITH", "INTEGER", "REAL", "BOOLEAN", "CHAR", "STRING",
    "TRUE", "FALSE", "WRITELN", "READLN", "WRITE", "READ",
};

static int in_table(const char *const *tab, size_t n, const char *w)
{
    for (size_t i = 0; i < n; i++)
        if (strcmp(tab[i], w) == 0)
            return 1;
    return 0;
}

static int is_keyword(const char *w, bc_lang lang)
{
    switch (lang) {
    case BC_LANG_C:
        return in_table(kw_c, sizeof kw_c / sizeof *kw_c, w);
    case BC_LANG_CPP:
        return in_table(kw_c, sizeof kw_c / sizeof *kw_c, w) ||
               in_table(kw_cpp, sizeof kw_cpp / sizeof *kw_cpp, w);
    case BC_LANG_JAVA:
        return in_table(kw_java, sizeof kw_java / sizeof *kw_java, w);
    case BC_LANG_PLX:
    case BC_LANG_PLAS:
        return in_table(kw_plx, sizeof kw_plx / sizeof *kw_plx, w);
    case BC_LANG_ASM:
        return in_table(kw_asm, sizeof kw_asm / sizeof *kw_asm, w);
    case BC_LANG_PASCAL:
        return in_table(kw_pascal, sizeof kw_pascal / sizeof *kw_pascal, w);
    default:
        return in_table(kw_c, sizeof kw_c / sizeof *kw_c, w);
    }
}

/* ------------------------------------------------------------- tokenizer */

static int id_start(unsigned char c)
{
    return isalpha(c) || c == '_' || c == '$' || c == '#' || c == '@';
}

static int id_char(unsigned char c)
{
    return id_start(c) || isdigit(c);
}

static void fold_up(char *s)
{
    for (; *s; s++)
        *s = (char)toupper((unsigned char)*s);
}

static int cmp_str(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

char **bc_extract_refs(const char *body, bc_lang lang, size_t *out_n)
{
    size_t cap = 64, n = 0;
    char **v = (char **)malloc(cap * sizeof *v);
    if (!v) {
        *out_n = 0;
        return NULL;
    }
    int fold = bc_lang_folds_case(lang);
    const char *p = body ? body : "";
    while (*p) {
        if (!id_start((unsigned char)*p)) {
            p++;
            continue;
        }
        const char *s = p;
        while (id_char((unsigned char)*p))
            p++;
        size_t len = (size_t)(p - s);
        if (len == 0 || len > 128)
            continue;
        char *w = (char *)malloc(len + 1);
        if (!w)
            break;
        memcpy(w, s, len);
        w[len] = 0;
        if (fold)
            fold_up(w);
        if (is_keyword(w, lang)) {
            free(w);
            continue;
        }
        if (n == cap) {
            cap *= 2;
            char **nv = (char **)realloc(v, cap * sizeof *v);
            if (!nv) {
                free(w);
                break;
            }
            v = nv;
        }
        v[n++] = w;
    }
    /* sort + dedupe for determinism */
    qsort(v, n, sizeof *v, cmp_str);
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        if (o && strcmp(v[o - 1], v[i]) == 0)
            free(v[i]);
        else
            v[o++] = v[i];
    }
    *out_n = o;
    return v;
}

void bc_refs_free(char **refs, size_t n)
{
    if (!refs)
        return;
    for (size_t i = 0; i < n; i++)
        free(refs[i]);
    free(refs);
}

/* ----------------------------------------------------------------- index */

typedef struct {
    const char *key;      /* arena copy, folded when applicable */
    const bc_decl *decl;
} islot;

struct bc_index {
    bc_arena A;
    islot *slots;
    size_t nslots;        /* power of two */
    bc_lang lang;
};

static size_t hash_str(const char *s)
{
    size_t h = 1469598103934665603u;
    for (; *s; s++)
        h = (h ^ (unsigned char)*s) * 1099511628211u;
    return h;
}

static void idx_put(bc_index *ix, const char *name, const bc_decl *d,
                    int overwrite)
{
    size_t m = ix->nslots - 1;
    size_t i = hash_str(name) & m;
    while (ix->slots[i].key) {
        if (strcmp(ix->slots[i].key, name) == 0) {
            if (overwrite)
                ix->slots[i].decl = d;
            return; /* first-wins unless overwrite */
        }
        i = (i + 1) & m;
    }
    ix->slots[i].key = name;
    ix->slots[i].decl = d;
}

bc_index *bc_index_build(const bc_decl *decls, size_t ndecls, bc_lang lang)
{
    bc_index *ix = (bc_index *)calloc(1, sizeof *ix);
    if (!ix)
        return NULL;
    ix->lang = lang;
    /* generous sizing: explicit names + tokenized text names */
    size_t est = 64;
    for (size_t i = 0; i < ndecls; i++)
        est += decls[i].nnames + strlen(decls[i].text ? decls[i].text : "") / 8;
    ix->nslots = 64;
    while (ix->nslots < est * 2)
        ix->nslots <<= 1;
    ix->slots = (islot *)calloc(ix->nslots, sizeof *ix->slots);
    if (!ix->slots) {
        free(ix);
        return NULL;
    }
    int fold = bc_lang_folds_case(lang);

    /* pass 1: explicit names (authoritative, first-wins between decls) */
    for (size_t i = 0; i < ndecls; i++) {
        for (size_t k = 0; k < decls[i].nnames; k++) {
            const char *nm = decls[i].names[k];
            if (!nm || !*nm)
                continue;
            char *key = bc_adup(&ix->A, nm, strlen(nm));
            if (fold)
                fold_up(key);
            idx_put(ix, key, &decls[i], 0);
        }
    }
    /* pass 2: identifiers tokenized from each decl's text — fills the
     * every-name rule (members, BASED pointers) when the parser
     * under-reports; never displaces an explicit name. */
    for (size_t i = 0; i < ndecls; i++) {
        size_t nrefs = 0;
        char **refs = bc_extract_refs(decls[i].text, lang, &nrefs);
        for (size_t k = 0; k < nrefs; k++) {
            char *key = bc_adup(&ix->A, refs[k], strlen(refs[k]));
            idx_put(ix, key, &decls[i], 0);
        }
        bc_refs_free(refs, nrefs);
    }
    return ix;
}

const bc_decl *bc_index_lookup(const bc_index *ix, const char *name)
{
    if (!ix || !name)
        return NULL;
    char tmp[132];
    size_t len = strlen(name);
    if (len >= sizeof tmp)
        return NULL;
    memcpy(tmp, name, len + 1);
    if (bc_lang_folds_case(ix->lang))
        fold_up(tmp);
    size_t m = ix->nslots - 1;
    size_t i = hash_str(tmp) & m;
    while (ix->slots[i].key) {
        if (strcmp(ix->slots[i].key, tmp) == 0)
            return ix->slots[i].decl;
        i = (i + 1) & m;
    }
    return NULL;
}

void bc_index_free(bc_index *ix)
{
    if (!ix)
        return;
    free(ix->slots);
    bc_arena_free(&ix->A);
    free(ix);
}

/* --------------------------------------------------------------- closure */

typedef struct {
    const bc_decl **v;
    size_t n, cap;
} dvec;

static int dvec_has(const dvec *d, const bc_decl *x)
{
    for (size_t i = 0; i < d->n; i++)
        if (d->v[i] == x)
            return 1;
    return 0;
}

static void dvec_push(dvec *d, const bc_decl *x)
{
    if (d->n == d->cap) {
        d->cap = d->cap ? d->cap * 2 : 16;
        const bc_decl **nv =
            (const bc_decl **)realloc((void *)d->v, d->cap * sizeof *nv);
        if (!nv)
            return;
        d->v = nv;
    }
    d->v[d->n++] = x;
}

const bc_decl **bc_closure(const char *body, const bc_index *idx,
                           bc_lang lang, size_t max_chars,
                           int transitive_depth, size_t *out_n)
{
    *out_n = 0;
    if (!idx)
        return NULL;
    if (max_chars == 0)
        max_chars = 4000;

    dvec seen = {0};   /* everything ever admitted (any tier) */
    dvec out = {0};    /* budget-approved output, tier order   */
    size_t used = 0;

    size_t nrefs = 0;
    char **refs = bc_extract_refs(body, lang, &nrefs);

    /* tier 0: direct references, alphabetical (refs are sorted) */
    dvec tier = {0};
    for (size_t i = 0; i < nrefs; i++) {
        const bc_decl *d = bc_index_lookup(idx, refs[i]);
        if (d && !dvec_has(&seen, d)) {
            dvec_push(&seen, d);
            dvec_push(&tier, d);
        }
    }
    bc_refs_free(refs, nrefs);

    for (int depth = 0; depth <= transitive_depth; depth++) {
        /* admit this tier under the budget; tier 0 never crowded out
         * because it is admitted first */
        int admitted_any = 0;
        for (size_t i = 0; i < tier.n; i++) {
            size_t len = strlen(tier.v[i]->text ? tier.v[i]->text : "");
            if (used + len <= max_chars || out.n == 0) {
                /* "never send zero context when context exists" */
                dvec_push(&out, tier.v[i]);
                used += len;
                admitted_any = 1;
            }
        }
        if (depth == transitive_depth || !admitted_any)
            break;
        /* next tier: refs of this tier's texts */
        dvec next = {0};
        for (size_t i = 0; i < tier.n; i++) {
            size_t nn = 0;
            char **trefs = bc_extract_refs(tier.v[i]->text, lang, &nn);
            for (size_t k = 0; k < nn; k++) {
                const bc_decl *d = bc_index_lookup(idx, trefs[k]);
                if (d && !dvec_has(&seen, d)) {
                    dvec_push(&seen, d);
                    dvec_push(&next, d);
                }
            }
            bc_refs_free(trefs, nn);
        }
        free((void *)tier.v);
        tier = next;
    }
    free((void *)tier.v);
    free((void *)seen.v);
    *out_n = out.n;
    return out.v;
}

/* --------------------------------------------------------------- snippet */

char *bc_build_snippet(const char *doc_brief, const bc_decl **closure,
                       size_t nclosure, const char **callee_lines,
                       size_t ncallees, bc_lang lang, const char *body)
{
    bc_sb sb = {0};
    if (doc_brief && *doc_brief) {
        bc_sb_adds(&sb, "DOC:\n");
        bc_sb_adds(&sb, doc_brief);
        bc_sb_adds(&sb, "\n\n");
    }
    if (nclosure) {
        bc_sb_adds(&sb, "DECLARATIONS:\n");
        for (size_t i = 0; i < nclosure; i++) {
            bc_sb_adds(&sb, closure[i]->text ? closure[i]->text : "");
            bc_sb_addc(&sb, '\n');
        }
        bc_sb_addc(&sb, '\n');
    }
    if (ncallees) {
        bc_sb_adds(&sb, "CALLEES:\n");
        for (size_t i = 0; i < ncallees; i++) {
            bc_sb_adds(&sb, callee_lines[i]);
            bc_sb_addc(&sb, '\n');
        }
        bc_sb_addc(&sb, '\n');
    }
    bc_sb_adds(&sb, "FUNCTION (");
    bc_sb_adds(&sb, bc_lang_display(lang));
    bc_sb_adds(&sb, "):\n");
    bc_sb_adds(&sb, body ? body : "");
    bc_sb_addc(&sb, '\n');
    return bc_sb_take(&sb);
}
