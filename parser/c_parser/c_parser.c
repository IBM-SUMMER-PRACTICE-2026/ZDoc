/*
 * ZDoc c_parser — implementation.
 *
 * Design for speed:
 *   - one forward pass over a padded copy of the input; no regex, no tokens
 *     stored, no backtracking
 *   - function bodies (the bulk of any source file) are skipped by a
 *     brace-matching loop driven by a 256-entry "interesting char" table
 *   - line numbers are computed lazily (memchr over the gap since the last
 *     query) so the hot loops never count newlines
 *   - output strings are individually heap-allocated; cp_result_free walks the
 *     symbols and doc blocks to release them
 */
#include "c_parser.h"
#include "../shared/parser_shared.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------- allocation */

/* Copy s[0..n) into a fresh heap buffer, NUL-terminated. Each output string
 * is individually owned and released in cp_result_free / free_doc. */
static char *dupn(const char *s, size_t n)
{
    char *d = (char *)malloc(n + 1);
    if (!d)
        return NULL;
    memcpy(d, s, n);
    d[n] = 0;
    return d;
}

/* Free every heap doc string a cp_symbol owns (all fields tolerate NULL). */
static void free_doc(cp_symbol *d)
{
    free((char *)d->brief);
    free((char *)d->returns);
    free((char *)d->notes);
    for (size_t i = 0; i < d->nparams; i++) {
        free((char *)d->params[i].name);
        free((char *)d->params[i].desc);
    }
    free(d->params);
}

/* -------------------------------------------------------- growable string */

typedef struct {
    char *d;
    size_t n, cap;
} sb;

static void sb_put(sb *s, const char *a, size_t n)
{
    if (!n)
        return;
    if (s->n + n + 1 > s->cap) {
        size_t c = s->cap ? s->cap * 2 : 64;
        while (c < s->n + n + 1)
            c *= 2;
        char *nd = (char *)realloc(s->d, c);
        if (!nd)
            return;
        s->d = nd;
        s->cap = c;
    }
    memcpy(s->d + s->n, a, n);
    s->n += n;
}

/* Append a line of text, joined to prior content with a single space. */
static void sb_join(sb *s, const char *a, const char *b)
{
    if (a >= b)
        return;
    if (s->n)
        sb_put(s, " ", 1);
    sb_put(s, a, (size_t)(b - a));
}

/* Trim trailing whitespace and hand the buffer to the caller (who now owns
 * it and must free()). Returns NULL for empty content. */
static const char *sb_done(sb *s)
{
    while (s->n && isspace((unsigned char)s->d[s->n - 1]))
        s->n--;
    const char *r;
    if (s->n) {
        s->d[s->n] = 0; /* sb_put always keeps room for the terminator */
        r = s->d;       /* transfer ownership - no copy */
    } else {
        free(s->d);
        r = NULL;
    }
    s->d = NULL;
    s->n = s->cap = 0;
    return r;
}

/* ----------------------------------------------------------------- result */

struct cp_result {
    cp_symbol *syms;
    size_t n, cap;
    char *buf;      /* padded scan buffer; freed as soon as parsing ends */
    const char *err;
    cp_symbol filedoc;
    int has_filedoc;
};

/* ----------------------------------------------------------- parser state */

typedef struct {
    const char *s, *e;
    int is_line; /* run of /// lines rather than a block */
    int valid;
} docref;

typedef struct {
    const char *begin, *end, *p;
    const char *anchor;      /* lazy line counting */
    uint32_t anchor_line;
    docref doc;              /* pending doc comment awaiting a declaration */
    int depth;               /* decl-scope nesting guard */
    cp_result *res;
} P;

typedef struct {
    const char *s, *e;
} span;

#define SPAN_OK(x) ((x).s != NULL && (x).e > (x).s)

static int spis(span sp, const char *lit)
{
    size_t n = strlen(lit);
    return sp.s && (size_t)(sp.e - sp.s) == n && memcmp(sp.s, lit, n) == 0;
}

static uint32_t line_at(P *st, const char *q)
{
    const char *s = st->anchor;
    uint32_t ln = st->anchor_line;
    while (s < q) {
        const char *nl = (const char *)memchr(s, '\n', (size_t)(q - s));
        if (!nl)
            break;
        ln++;
        s = nl + 1;
    }
    st->anchor = q;
    st->anchor_line = ln;
    return ln;
}

/* --------------------------------------------------------------- char sets */

static int is_id_start(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' ||
           c >= 0x80;
}

static int is_id_char(unsigned char c)
{
    return is_id_start(c) || (c >= '0' && c <= '9');
}

/* Names that can never be the identifier naming a function. */
static const char *const reject_kw[] = {
    "alignas", "alignof", "asm", "auto", "bool", "case", "catch", "char",
    "class", "const", "const_cast", "constexpr", "decltype", "default",
    "defined", "delete", "do", "double", "dynamic_cast", "else", "enum",
    "explicit", "extern", "float", "for", "friend", "goto", "if", "inline",
    "int", "long", "mutable", "namespace", "new", "noexcept", "offsetof",
    "private", "protected", "public", "register", "reinterpret_cast",
    "requires", "restrict", "return", "short", "signed", "sizeof", "static",
    "static_assert", "static_cast", "struct", "switch", "template", "throw",
    "typedef", "typeid", "typename", "typeof", "union", "unsigned", "using",
    "virtual", "void", "volatile", "while", "_Alignof", "_Bool",
    "_Static_assert", "__asm", "__asm__", "__attribute__", "__declspec",
    "__typeof__",
};

static int is_kw(span sp)
{
    size_t n = (size_t)(sp.e - sp.s);
    for (size_t i = 0; i < sizeof reject_kw / sizeof *reject_kw; i++) {
        const char *k = reject_kw[i];
        if (k[0] == sp.s[0] && strlen(k) == n && memcmp(k, sp.s, n) == 0)
            return 1;
    }
    return 0;
}

/* Statement-macro invocation at file scope, e.g. G_DEFINE_TYPE(...);  */
static int is_macroish(span nm, const char *stmt_start)
{
    if (nm.s != stmt_start || (size_t)(nm.e - nm.s) < 2)
        return 0;
    for (const char *q = nm.s; q < nm.e; q++)
        if (!((*q >= 'A' && *q <= 'Z') || (*q >= '0' && *q <= '9') || *q == '_'))
            return 0;
    return 1;
}

/* -------------------------------------------------------------- low skips */

static void skip_block(P *st) /* p is just past the opening slash-star */
{
    const char *p = st->p, *end = st->end;
    for (;;) {
        const char *q = (const char *)memchr(p, '*', (size_t)(end - p));
        if (!q) {
            st->p = end;
            return;
        }
        if (q[1] == '/') { /* padded buffer makes q[1] safe */
            st->p = q + 2;
            return;
        }
        p = q + 1;
    }
}

static void skip_line_comment(P *st) /* p at first slash */
{
    const char *p = st->p, *end = st->end;
    while (p < end) {
        if (*p == '\n') {
            if (p[-1] == '\\' || (p[-1] == '\r' && p - 2 >= st->begin && p[-2] == '\\')) {
                p++;
                continue;
            }
            break;
        }
        p++;
    }
    st->p = p; /* leave the newline for the whitespace skipper */
}

static void skip_string(P *st) /* p at opening quote */
{
    const char *p = st->p, *end = st->end;
    if (p > st->begin && p[-1] == 'R') { /* C++ raw string R"delim( ... )delim" */
        const char *d = p + 1, *paren = d;
        while (paren < end && *paren != '(' && *paren != '"' &&
               *paren != '\n' && paren - d < 18)
            paren++;
        if (paren < end && *paren == '(') {
            size_t dn = (size_t)(paren - d);
            const char *q = paren + 1;
            for (;;) {
                q = (const char *)memchr(q, ')', (size_t)(end - q));
                if (!q) {
                    st->p = end;
                    return;
                }
                if (q + 1 + dn < end && memcmp(q + 1, d, dn) == 0 && q[1 + dn] == '"') {
                    st->p = q + dn + 2;
                    return;
                }
                q++;
            }
        }
    }
    p++;
    while (p < end) {
        char c = *p;
        if (c == '\\')
            p += 2;
        else if (c == '"') {
            p++;
            break;
        } else if (c == '\n')
            break; /* unterminated: be tolerant */
        else
            p++;
    }
    st->p = p;
}

static void skip_char(P *st) /* p at opening quote */
{
    const char *p = st->p + 1, *end = st->end;
    while (p < end) {
        char c = *p;
        if (c == '\\')
            p += 2;
        else if (c == '\'') {
            p++;
            break;
        } else if (c == '\n')
            break;
        else
            p++;
    }
    st->p = p;
}

static void skip_pp_line(P *st) /* p at '#'; stops at the logical EOL */
{
    const char *p = st->p, *end = st->end;
    while (p < end) {
        char c = *p;
        if (c == '\n') {
            if (p[-1] == '\\' || (p[-1] == '\r' && p - 2 >= st->begin && p[-2] == '\\')) {
                p++;
                continue;
            }
            break;
        }
        if (c == '/' && p[1] == '*') {
            st->p = p + 2;
            skip_block(st);
            p = st->p;
            continue;
        }
        if (c == '/' && p[1] == '/') {
            const char *nl = (const char *)memchr(p, '\n', (size_t)(end - p));
            p = nl ? nl : end;
            break;
        }
        p++;
    }
    st->p = p;
}

/* Fast body skipper: everything between { } that isn't a declaration we
 * care about. This loop sees the majority of the input bytes. */
static void skip_body(P *st) /* p at '{' */
{
    static const unsigned char interesting[256] = {
        ['{'] = 1, ['}'] = 1, ['"'] = 1, ['\''] = 1, ['/'] = 1, ['#'] = 1,
    };
    const char *end = st->end;
    int depth = 0;
    while (st->p < end) {
        const char *p = st->p;
        while (p < end && !interesting[(unsigned char)*p])
            p++;
        st->p = p;
        if (p >= end)
            return;
        char c = *p;
        switch (c) {
        case '{':
            depth++;
            st->p++;
            break;
        case '}':
            depth--;
            st->p++;
            if (depth <= 0)
                return;
            break;
        case '"':
            skip_string(st);
            break;
        case '\'':
            /* C++14 digit separator (1'000'000) — prev char alnum */
            if (p > st->begin && isalnum((unsigned char)p[-1]))
                st->p++;
            else
                skip_char(st);
            break;
        case '/':
            if (p[1] == '*') {
                st->p = p + 2;
                skip_block(st);
            } else if (p[1] == '/') {
                skip_line_comment(st);
            } else {
                st->p++;
            }
            break;
        case '#':
            skip_pp_line(st); /* guards apostrophes in #error etc. */
            break;
        }
    }
}

/* Whitespace + comments inside a statement; leaves pending doc alone. */
static void ws_quiet(P *st)
{
    const char *end = st->end;
    for (;;) {
        while (st->p < end && isspace((unsigned char)*st->p))
            st->p++;
        if (st->p[0] == '/' && st->p[1] == '*') {
            st->p += 2;
            skip_block(st);
        } else if (st->p[0] == '/' && st->p[1] == '/') {
            skip_line_comment(st);
        } else {
            return;
        }
    }
}

static int parse_doc_text(P *st, docref d, cp_symbol *out);

/* A doc block carrying @file/@mainpage documents the module, not the next
 * symbol. Detect it as soon as the block is scanned so it can't mis-attach. */
static int maybe_filedoc(P *st, docref d)
{
    for (const char *q = d.s; q + 5 <= d.e; q++) {
        if ((*q == '@' || *q == '\\') &&
            ((memcmp(q + 1, "file", 4) == 0 && !isalpha((unsigned char)q[5])) ||
             (q + 9 <= d.e && memcmp(q + 1, "mainpage", 8) == 0))) {
            cp_symbol scratch;
            parse_doc_text(st, d, &scratch); /* stores into res->filedoc */
            return 1;
        }
    }
    return 0;
}

/* Whitespace + comments at declaration scope; tracks doc comments.
 * Doc forms: slash-star-star, slash-star-bang blocks and runs of /// or //!
 * lines. A plain comment between a doc block and the declaration breaks
 * the association. */
static void ws_and_docs(P *st)
{
    const char *end = st->end;
    for (;;) {
        while (st->p < end && isspace((unsigned char)*st->p))
            st->p++;
        if (st->p[0] == '/' && st->p[1] == '*') {
            const char *cs = st->p;
            int isdoc = (st->p[2] == '*' || st->p[2] == '!') &&
                        !(st->p[2] == '*' && st->p[3] == '/');
            st->p += 2;
            skip_block(st);
            if (isdoc) {
                docref d = {cs, st->p, 0, 1};
                if (maybe_filedoc(st, d))
                    st->doc.valid = 0;
                else
                    st->doc = d;
            } else {
                st->doc.valid = 0;
            }
        } else if (st->p[0] == '/' && st->p[1] == '/') {
            const char *cs = st->p;
            int isdoc = (st->p[2] == '/' || st->p[2] == '!');
            skip_line_comment(st);
            if (isdoc) {
                if (st->doc.valid && st->doc.is_line) {
                    st->doc.e = st->p; /* extend the run */
                } else {
                    docref d = {cs, st->p, 1, 1};
                    if (maybe_filedoc(st, d))
                        st->doc.valid = 0;
                    else
                        st->doc = d;
                }
            } else {
                st->doc.valid = 0;
            }
        } else {
            return;
        }
    }
}

/* ----------------------------------------------------------- doc parsing */

static int tag_is(const char *s, size_t n, const char *lit)
{
    size_t ln = strlen(lit);
    if (n != ln)
        return 0;
    for (size_t i = 0; i < n; i++)
        if (tolower((unsigned char)s[i]) != lit[i])
            return 0;
    return 1;
}

typedef struct {
    sb name, desc;
} draft_param;

/* Parse the doc-comment text into `out`. Returns 1 when the doc attaches
 * to the symbol; 0 for file-level docs (@file/@mainpage), which are stored
 * on the result instead. */
static int parse_doc_text(P *st, docref d, cp_symbol *out)
{
    const char *s = d.s, *e = d.e;
    if (!d.is_line) {
        s += 3; /* past the opener incl. the doc marker char */
        if (e - s >= 2)
            e -= 2; /* drop the closing star-slash */
    }

    sb brief = {0}, rets = {0}, notes = {0};
    draft_param *dp = NULL;
    size_t np = 0, cp_ = 0;
    enum { F_BRIEF, F_PARAM, F_RET, F_NOTE } cur = F_BRIEF;
    int is_filedoc = 0;

    const char *ls = s;
    while (ls < e) {
        const char *le = (const char *)memchr(ls, '\n', (size_t)(e - ls));
        if (!le)
            le = e;
        const char *a = ls, *b = le;
        ls = le + 1;

        while (a < b && isspace((unsigned char)*a))
            a++;
        if (d.is_line) {
            while (a < b && *a == '/')
                a++;
            if (a < b && *a == '!')
                a++;
        } else {
            while (a < b && *a == '*')
                a++;
            if (a < b && *a == '!')
                a++;
        }
        if (a < b && (*a == ' ' || *a == '\t'))
            a++;
        while (b > a && isspace((unsigned char)b[-1]))
            b--;

        if (a >= b) { /* blank/banner line: paragraph break */
            if (cur == F_BRIEF && brief.n)
                cur = F_NOTE;
            continue;
        }

        if (*a == '@' || *a == '\\') {
            const char *t = a + 1, *tw = t;
            while (tw < b && isalpha((unsigned char)*tw))
                tw++;
            size_t tn = (size_t)(tw - t);
            const char *r = tw;
            while (r < b && isspace((unsigned char)*r))
                r++;

            if (tag_is(t, tn, "brief") || tag_is(t, tn, "short")) {
                cur = F_BRIEF;
                sb_join(&brief, r, b);
            } else if (tag_is(t, tn, "param") || tag_is(t, tn, "tparam")) {
                if (r < b && *r == '[') {
                    const char *x = (const char *)memchr(r, ']', (size_t)(b - r));
                    r = x ? x + 1 : b;
                    while (r < b && isspace((unsigned char)*r))
                        r++;
                }
                const char *nsrt = r;
                while (r < b && !isspace((unsigned char)*r))
                    r++;
                if (np == cp_) {
                    size_t ncp = cp_ ? cp_ * 2 : 8;
                    draft_param *ndp =
                        (draft_param *)realloc(dp, ncp * sizeof *dp);
                    if (!ndp)
                        continue; /* OOM: drop this @param, keep the rest */
                    dp = ndp;
                    cp_ = ncp;
                }
                memset(&dp[np], 0, sizeof dp[np]);
                sb_put(&dp[np].name, nsrt, (size_t)(r - nsrt));
                while (r < b && isspace((unsigned char)*r))
                    r++;
                sb_join(&dp[np].desc, r, b);
                np++;
                cur = F_PARAM;
            } else if (tag_is(t, tn, "return") || tag_is(t, tn, "returns") ||
                       tag_is(t, tn, "result") || tag_is(t, tn, "retval")) {
                cur = F_RET;
                sb_join(&rets, r, b);
            } else if (tag_is(t, tn, "file") || tag_is(t, tn, "mainpage") ||
                       tag_is(t, tn, "page")) {
                is_filedoc = 1;
                cur = F_BRIEF; /* the tag argument is just the filename */
            } else if (tag_is(t, tn, "note") || tag_is(t, tn, "details") ||
                       tag_is(t, tn, "remark") || tag_is(t, tn, "remarks")) {
                cur = F_NOTE;
                sb_join(&notes, r, b);
            } else { /* unknown tag: keep it verbatim under notes */
                cur = F_NOTE;
                sb_join(&notes, a, b);
            }
        } else {
            switch (cur) {
            case F_BRIEF:
                sb_join(&brief, a, b);
                break;
            case F_PARAM:
                sb_join(&dp[np - 1].desc, a, b);
                break;
            case F_RET:
                sb_join(&rets, a, b);
                break;
            case F_NOTE:
                sb_join(&notes, a, b);
                break;
            }
        }
    }

    cp_symbol doc = {0};
    doc.brief = sb_done(&brief);
    doc.returns = sb_done(&rets);
    doc.notes = sb_done(&notes);
    if (np) {
        doc.params = (cp_doc_param *)malloc(np * sizeof *doc.params);
        if (doc.params) {
            for (size_t i = 0; i < np; i++) {
                doc.params[i].name = sb_done(&dp[i].name);
                doc.params[i].desc = sb_done(&dp[i].desc);
            }
            doc.nparams = np;
        } else {
            /* OOM: drain the draft buffers so they don't leak */
            for (size_t i = 0; i < np; i++) {
                free((char *)sb_done(&dp[i].name));
                free((char *)sb_done(&dp[i].desc));
            }
        }
    }
    free(dp);

    if (is_filedoc) {
        if (!st->res->has_filedoc) {
            st->res->filedoc = doc;
            st->res->has_filedoc = 1;
        } else {
            /* only the first @file/@mainpage is kept; free the rest */
            free_doc(&doc);
        }
        memset(out, 0, sizeof *out);
        return 0;
    }
    *out = doc;
    return doc.brief || doc.returns || doc.notes || doc.nparams;
}

/* ------------------------------------------------------------- emission */

/* Copy [a,b) collapsing whitespace runs and stripping comments. Returns an
 * owned heap string, or NULL on allocation failure. */
static char *make_sig(const char *a, const char *b)
{
    char *out = (char *)malloc((size_t)(b - a) + 1);
    if (!out)
        return NULL;
    size_t o = 0;
    int ws = 0;
    const char *p = a;
    while (p < b) {
        char c = *p;
        if (c == '/' && p + 1 < b && p[1] == '*') {
            p += 2;
            while (p + 1 < b && !(p[0] == '*' && p[1] == '/'))
                p++;
            p = (p + 2 < b) ? p + 2 : b;
            ws = 1;
            continue;
        }
        if (c == '/' && p + 1 < b && p[1] == '/') {
            while (p < b && *p != '\n')
                p++;
            ws = 1;
            continue;
        }
        if (c == '\\' && p + 1 < b && p[1] == '\n') {
            p += 2;
            ws = 1;
            continue;
        }
        if (isspace((unsigned char)c)) {
            ws = 1;
            p++;
            continue;
        }
        if (ws && o)
            out[o++] = ' ';
        ws = 0;
        out[o++] = c;
        p++;
    }
    out[o] = 0;
    return out;
}

static void emit(P *st, cp_symbol_kind k, span nm, const char *ss,
                 const char *se, uint32_t line, docref *doc)
{
    cp_result *r = st->res;
    if (r->n == r->cap) {
        size_t c = r->cap ? r->cap * 2 : 64;
        cp_symbol *ns = (cp_symbol *)realloc(r->syms, c * sizeof *ns);
        if (!ns)
            return;
        r->syms = ns;
        r->cap = c;
    }
    cp_symbol *sym = &r->syms[r->n++];
    memset(sym, 0, sizeof *sym);
    sym->kind = k;
    sym->line = line;
    sym->name = dupn(nm.s, (size_t)(nm.e - nm.s));
    sym->signature = make_sig(ss, se);
    if (doc->valid) {
        cp_symbol d = {0};
        if (parse_doc_text(st, *doc, &d)) {
            sym->brief = d.brief;
            sym->returns = d.returns;
            sym->notes = d.notes;
            sym->params = d.params;
            sym->nparams = d.nparams;
        }
        doc->valid = 0;
    }
}

/* --------------------------------------------------------- decl scanning */

static void parse_decl_scope(P *st, int nested);

/* #directives at declaration scope. #define becomes a MACRO symbol. */
static void handle_pp(P *st)
{
    const char *hash = st->p;
    st->p++;
    while (*st->p == ' ' || *st->p == '\t')
        st->p++;
    const char *w = st->p;
    while (is_id_char((unsigned char)*st->p))
        st->p++;
    size_t wn = (size_t)(st->p - w);

    if (wn == 6 && memcmp(w, "define", 6) == 0) {
        docref doc = st->doc;
        st->doc.valid = 0;
        while (*st->p == ' ' || *st->p == '\t')
            st->p++;
        const char *ns = st->p;
        while (is_id_char((unsigned char)*st->p))
            st->p++;
        span nm = {ns, st->p};
        int fnlike = (*st->p == '(');
        uint32_t line = line_at(st, hash);
        const char *sig_end = NULL;
        if (fnlike) {
            int d = 0;
            const char *q = st->p;
            while (q < st->end) {
                if (*q == '(') {
                    d++;
                } else if (*q == ')') {
                    if (--d == 0) {
                        q++;
                        break;
                    }
                } else if (*q == '\n' && q[-1] != '\\') {
                    break;
                }
                q++;
            }
            sig_end = q;
            st->p = q;
        }
        skip_pp_line(st);
        if (!fnlike) {
            sig_end = st->p;
            if (sig_end - hash > 160)
                sig_end = hash + 160; /* long replacement lists add no value */
        }
        /* function-like macros are API; object-like only when documented
         * (this also drops include guards) */
        if (SPAN_OK(nm) && (fnlike || doc.valid))
            emit(st, CP_SYM_MACRO, nm, hash, sig_end, line, &doc);
    } else {
        if (wn == 7 && memcmp(w, "include", 7) == 0)
            st->doc.valid = 0;
        skip_pp_line(st);
    }
}

/* One declaration-scope statement: everything up to ';', a matched body,
 * or the enclosing '}'. This is where functions, prototypes, types and
 * documented variables are recognized. */
static void parse_statement(P *st)
{
    docref doc = st->doc;
    st->doc.valid = 0;

    const char *stmt_start = st->p;
    uint32_t line = line_at(st, stmt_start);
    span last_ident = {0, 0}, name = {0, 0}, tag_name = {0, 0}, var_ident = {0, 0};
    span td_inner = {0, 0}; /* declarator inside parens: typedef int (*cb)(...) */
    const char *sig_end_override = NULL;
    const char *chain_start = NULL, *tilde_pos = NULL;
    int pd = 0;          /* combined ()/[] nesting */
    int funcish = 0;     /* saw ident + balanced arg list */
    int has_init = 0;    /* '=' before funcish: variable initializer */
    int emitted = 0;
    int in_ctor = 0;     /* past the ':' of a constructor init list */
    int kw_typedef = 0, kw_record = 0, kw_enum = 0, kw_ns = 0, kw_using = 0;
    int kw_ext = 0, ext_lang = 0;
    int want_tag = 0, ntok = 0;
    enum { T0, TIDENT, TSCOPE, TTILDE, TOTHER } prev = T0;

    for (;;) {
        ws_quiet(st);
        if (st->p >= st->end)
            return;
        const char *tp = st->p;
        unsigned char c = (unsigned char)*tp;

        if (is_id_start(c)) {
            st->p++;
            while (is_id_char((unsigned char)*st->p))
                st->p++;
            span id = {tp, st->p};
            ntok++;
            if (pd == 0 && !funcish) {
                if (spis(id, "typedef")) {
                    kw_typedef = 1;
                } else if (spis(id, "struct") || spis(id, "class") ||
                           spis(id, "union")) {
                    kw_record = 1;
                    want_tag = 1;
                } else if (spis(id, "enum")) {
                    kw_enum = 1;
                    want_tag = 1;
                } else if (spis(id, "namespace")) {
                    kw_ns = 1;
                    want_tag = 1;
                } else if (spis(id, "using")) {
                    kw_using = 1;
                    want_tag = 1;
                } else if (spis(id, "extern")) {
                    kw_ext = 1;
                } else if (want_tag) {
                    tag_name = id;
                    want_tag = 0;
                }
            }
            /* qualified names (Foo::bar) and destructors (~Foo) */
            if (prev == TSCOPE && chain_start)
                id.s = chain_start;
            else if (prev == TTILDE && tilde_pos)
                id.s = tilde_pos;
            /* operator overloads: swallow the operator characters */
            if (spis((span){tp, st->p}, "operator")) {
                const char *q = st->p;
                while (*q == ' ' || *q == '\t')
                    q++;
                if (strchr("+-*/%^&|~!=<>,", *q)) {
                    const char *o = q;
                    while (strchr("+-*/%^&|~!=<>,", *q))
                        q++;
                    (void)o;
                    id.e = q;
                    st->p = q;
                } else if ((q[0] == '(' && q[1] == ')') ||
                           (q[0] == '[' && q[1] == ']')) {
                    id.e = q + 2;
                    st->p = q + 2;
                }
            }
            last_ident = id;
            prev = TIDENT;
            tilde_pos = NULL;
            continue;
        }

        st->p++;
        switch (c) {
        case ':':
            if (*st->p == ':') {
                st->p++;
                if (prev == TIDENT) {
                    chain_start = last_ident.s;
                    prev = TSCOPE;
                } else {
                    prev = TOTHER;
                }
            } else {
                if (pd == 0) {
                    if (funcish && !in_ctor) {
                        in_ctor = 1;
                        sig_end_override = tp;
                    } else if (!funcish && ntok == 1 &&
                               (spis(last_ident, "public") ||
                                spis(last_ident, "private") ||
                                spis(last_ident, "protected"))) {
                        /* access label: hand back to decl scope so the
                         * next member's doc comment is tracked */
                        st->doc = doc;
                        return;
                    }
                }
                prev = TOTHER;
            }
            ntok++;
            break;

        case '~':
            tilde_pos = (prev == TSCOPE && chain_start) ? chain_start : tp;
            prev = TTILDE;
            ntok++;
            break;

        case '(':
        case '[':
            if (pd == 0 && c == '(' && !funcish)
                name = (prev == TIDENT) ? last_ident : (span){0, 0};
            pd++;
            prev = TOTHER;
            ntok++;
            break;

        case ')':
        case ']':
            if (pd > 0)
                pd--;
            if (pd == 0 && c == ')' && !funcish) {
                if (!SPAN_OK(td_inner))
                    td_inner = last_ident;
                if (SPAN_OK(name) && !is_kw(name))
                    funcish = 1;
                else
                    name = (span){0, 0};
            }
            prev = TOTHER;
            ntok++;
            break;

        case '=':
            if (*st->p == '=') { /* comparison, not init */
                st->p++;
                prev = TOTHER;
                break;
            }
            {
                char pc = tp > st->begin ? tp[-1] : 0;
                /* skip compound assigns / comparisons (+=, <=, !=, ...) */
                if (!pc || !strchr("+-*/%&|^!<>", pc)) {
                    if (pd == 0 && !funcish && !has_init) {
                        has_init = 1;
                        var_ident = last_ident;
                    }
                    /* '=' after funcish is = 0 / = default / = delete */
                }
            }
            prev = TOTHER;
            ntok++;
            break;

        case ';':
            if (pd > 0) {
                prev = TOTHER;
                break;
            }
            if (!emitted) {
                if (kw_typedef) {
                    span tn = (funcish && SPAN_OK(name)) ? name
                              : SPAN_OK(td_inner)        ? td_inner
                                                         : last_ident;
                    if (SPAN_OK(tn) && !is_kw(tn))
                        emit(st, CP_SYM_TYPE, tn, stmt_start, tp, line, &doc);
                } else if (kw_using) {
                    if (has_init && SPAN_OK(tag_name))
                        emit(st, CP_SYM_TYPE, tag_name, stmt_start, tp, line, &doc);
                } else if (funcish && !has_init) {
                    if (!is_macroish(name, stmt_start))
                        emit(st, CP_SYM_PROTOTYPE, name, stmt_start, tp, line, &doc);
                } else if ((kw_record || kw_enum) && SPAN_OK(tag_name)) {
                    if (doc.valid) /* documented forward decl */
                        emit(st, CP_SYM_TYPE, tag_name, stmt_start, tp, line, &doc);
                } else if (doc.valid) { /* documented variable */
                    span v = SPAN_OK(var_ident) ? var_ident : last_ident;
                    if (SPAN_OK(v) && !is_kw(v))
                        emit(st, CP_SYM_VARIABLE, v, stmt_start, tp, line, &doc);
                }
            }
            return;

        case '{':
            if (pd > 0) { /* lambda in a default argument, etc. */
                st->p = tp;
                skip_body(st);
                prev = TOTHER;
                break;
            }
            if (kw_typedef || (has_init && !funcish)) {
                /* typedef struct {...} X;  or  = { initializer } */
                st->p = tp;
                skip_body(st);
                prev = TOTHER;
                break;
            }
            if (in_ctor && prev == TIDENT) { /* member brace-init: a{1} */
                st->p = tp;
                skip_body(st);
                prev = TOTHER;
                break;
            }
            if (kw_enum) {
                if (SPAN_OK(tag_name))
                    emit(st, CP_SYM_TYPE, tag_name, stmt_start, tp, line, &doc);
                emitted = 1;
                st->p = tp;
                skip_body(st);
                prev = TOTHER;
                break;
            }
            if (funcish) {
                const char *se = sig_end_override ? sig_end_override : tp;
                emit(st, CP_SYM_FUNCTION, name, stmt_start, se, line, &doc);
                st->p = tp;
                skip_body(st);
                return;
            }
            if (kw_record) {
                if (SPAN_OK(tag_name))
                    emit(st, CP_SYM_TYPE, tag_name, stmt_start, tp, line, &doc);
                emitted = 1;
                if (st->depth < 128)
                    parse_decl_scope(st, 1); /* members / methods */
                else {
                    st->p = tp;
                    skip_body(st);
                }
                prev = TOTHER;
                break;
            }
            if (kw_ns || (kw_ext && ext_lang)) { /* namespace / extern "C" */
                parse_decl_scope(st, 1);
                return;
            }
            st->p = tp;
            skip_body(st);
            prev = TOTHER;
            break;

        case '}':
            if (pd == 0) {
                st->p = tp; /* let the enclosing scope consume it */
                return;
            }
            prev = TOTHER;
            break;

        case '#':
            st->p = tp;
            skip_pp_line(st);
            break;

        case '"':
            st->p = tp;
            skip_string(st);
            if (kw_ext)
                ext_lang = 1; /* extern "C" */
            prev = TOTHER;
            ntok++;
            break;

        case '\'':
            if (tp > st->begin && isalnum((unsigned char)tp[-1])) {
                /* digit separator: quote already consumed */
            } else {
                st->p = tp;
                skip_char(st);
            }
            prev = TOTHER;
            break;

        default:
            prev = TOTHER;
            break;
        }
    }
}

static void parse_decl_scope(P *st, int nested)
{
    st->depth++;
    for (;;) {
        ws_and_docs(st);
        if (st->p >= st->end)
            break;
        char c = *st->p;
        if (c == '}') {
            st->p++;
            if (nested)
                break;
            st->doc.valid = 0; /* stray brace at top level */
            continue;
        }
        if (c == '#') {
            handle_pp(st);
            continue;
        }
        if (c == ';') {
            st->p++;
            st->doc.valid = 0;
            continue;
        }
        parse_statement(st);
    }
    st->depth--;
}

/* -------------------------------------------------------------- public API */

/* Scan r->buf[0..len) (which must already carry the 16-byte NUL padding) and
 * release the buffer as soon as the pass finishes: every output string is an
 * owned heap copy, so nothing references r->buf once parsing returns. This is
 * what keeps a parsed result from pinning a whole file-sized allocation. */
static void run_scan(cp_result *r, size_t len)
{
    P st = {0};
    st.begin = r->buf;
    st.end = r->buf + len;
    st.p = r->buf;
    if (len >= 3 && (unsigned char)r->buf[0] == 0xEF &&
        (unsigned char)r->buf[1] == 0xBB && (unsigned char)r->buf[2] == 0xBF)
        st.p += 3; /* UTF-8 BOM */
    st.anchor = st.p;
    st.anchor_line = 1;
    st.res = r;
    parse_decl_scope(&st, 0);

    free(r->buf);
    r->buf = NULL;
}

cp_result *cp_parse_buffer(const char *src, size_t len)
{
    cp_result *r = (cp_result *)calloc(1, sizeof *r);
    if (!r)
        return NULL;
    r->buf = (char *)malloc(len + 16);
    if (!r->buf) {
        r->err = "out of memory";
        return r;
    }
    memcpy(r->buf, src, len);
    memset(r->buf + len, 0, 16); /* NUL padding: lookahead never overruns */
    run_scan(r, len);
    return r;
}

cp_result *cp_parser(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        cp_result *r = (cp_result *)calloc(1, sizeof *r);
        if (r)
            r->err = strerror(errno);
        return r;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        cp_result *r = (cp_result *)calloc(1, sizeof *r);
        if (r)
            r->err = "unseekable file";
        return r;
    }
    fseek(f, 0, SEEK_SET);

    cp_result *r = (cp_result *)calloc(1, sizeof *r);
    if (!r) {
        fclose(f);
        return NULL;
    }
    /* Read straight into the padded scan buffer - no intermediate copy. */
    r->buf = (char *)malloc((size_t)sz + 16);
    if (!r->buf) {
        fclose(f);
        r->err = "out of memory";
        return r;
    }
    size_t rd = fread(r->buf, 1, (size_t)sz, f);
    fclose(f);
    memset(r->buf + rd, 0, 16); /* NUL padding from the actual bytes read */
    run_scan(r, rd);
    return r;
}

const cp_symbol *cp_symbols(const cp_result *r, size_t *count)
{
    if (count)
        *count = r ? r->n : 0;
    return r ? r->syms : NULL;
}

int cp_module_doc(const cp_result *r, cp_symbol *out)
{
    if (!r || !r->has_filedoc)
        return 0;
    if (out)
        *out = r->filedoc;
    return 1;
}

const char *cp_error(const cp_result *r)
{
    return r ? r->err : "out of memory";
}

const char *cp_symbol_kind_name(cp_symbol_kind k)
{
    switch (k) {
    case CP_SYM_FUNCTION:  return "function";
    case CP_SYM_PROTOTYPE: return "prototype";
    case CP_SYM_MACRO:     return "macro";
    case CP_SYM_TYPE:      return "type";
    case CP_SYM_VARIABLE:  return "variable";
    }
    return "unknown";
}

void cp_result_free(cp_result *r)
{
    if (!r)
        return;
    for (size_t i = 0; i < r->n; i++) {
        cp_symbol *s = &r->syms[i];
        free((char *)s->name);
        free((char *)s->signature);
        free_doc(s);
    }
    if (r->has_filedoc)
        free_doc(&r->filedoc);
    free(r->syms);
    free(r->buf);
    free(r);
}

/* ------------------------------------------------ shared Module adapter */

/* Duplicate a C string with dupn (reuses the module's owned-string helper). */
static char *dupcstr(const char *s)
{
    return s ? dupn(s, strlen(s)) : NULL;
}

/* Move every owned string out of a parsed cp_result into a shared Module.
 * Strings are transferred (not copied); the cp_symbol fields are cleared so
 * cp_result_free then tears down only the leftovers (the emptied doc blocks,
 * the file-level doc we drop, the symbol array and the result itself).
 *
 * Field mapping: kind -> type (as a string), brief -> description,
 * returns -> output, notes -> notes, params -> input. The
 * C-only module doc has no shared home and is dropped. */
static Module *result_to_module(cp_result *r, const char *path)
{
    Module *m = init_module(path);

    if (r->n) {
        for (size_t i = 0; i < r->n; i++) {
            cp_symbol *cs = &r->syms[i];
            Symbol *sy = module_add_symbol(m);

            sy->name = (char *)cs->name;
            cs->name = NULL;
            sy->signature = (char *)cs->signature;
            cs->signature = NULL;
            sy->line = cs->line;
            sy->type = dupcstr(cp_symbol_kind_name(cs->kind));
            sy->diagram = NULL;

            sy->description = (char *)cs->brief;
            cs->brief = NULL;
            sy->output = (char *)cs->returns;
            cs->returns = NULL;
            sy->notes = (char *)cs->notes;
            cs->notes = NULL;

            for (size_t j = 0; j < cs->nparams; j++)
                symbol_add_input(sy, cs->params[j].name,
                                  cs->params[j].desc);
        }
    }

    module_shrink_to_fit(m);
    cp_result_free(r);
    return m;
}

Module *cp_parse_file(const char *path)
{
    cp_result *r = cp_parser(path);
    if (!r)
        return NULL; /* allocation failure */

    if (cp_error(r)) {
        fprintf(stderr, "zdoc-c-parser: %s: %s\n", path, cp_error(r));
        cp_result_free(r);
        return init_module(path); /* empty module */
    }

    return result_to_module(r, path);
}