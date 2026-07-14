#include "parser_helpers.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



/*********************************************/
/*          PLXMAC PROLOG PARSING            */
/*********************************************/

/**
 * @brief Detect the banner line that opens a Method Prolog block.
 *
 * Method Prolog blocks (see docs/plx-doccomment-convention.md, "Method
 * Prolog Blocks") are one multi-line comment with quirky listing-border
 * delimiters, so the start is recognized by the "Start of Method Prolog"
 * banner text, not the delimiter bytes.
 *
 * @param line The line to test.
 * @return Non-zero if line opens a Method Prolog block, 0 otherwise.
 */
int is_prolog_start(Line line)
{
    const char *end = line.data + line.len;
    const char *s = skip_ws_n(line.data, end);
    if (s >= end || (*s != '*' && *s != '/'))
        return 0;
    return contains_ci(s, end, "Start of Method Prolog");
}

/**
 * @brief Detect the banner line that closes a Method Prolog block.
 *
 * Mirrors is_prolog_start(): recognized by the "End of Method Prolog"
 * banner text rather than the comment delimiter bytes.
 *
 * @param line The line to test.
 * @return Non-zero if line closes a Method Prolog block, 0 otherwise.
 */
int is_prolog_end(Line line)
{
    const char *end = line.data + line.len;
    const char *s = skip_ws_n(line.data, end);
    if (s >= end || (*s != '*' && *s != '/'))
        return 0;
    return contains_ci(s, end, "End of Method Prolog");
}

/**
 * @brief Strip the leading '*' box border from a Method Prolog interior
 *        line.
 *
 * @param line One interior line of a Method Prolog block.
 * @return The trimmed content as a slice into line's backing buffer; an
 *         empty (zero-length) slice for a padding-only line.
 */
Line prolog_content(Line line)
{
    const char *end = line.data + line.len;
    const char *s = skip_ws_n(line.data, end);

    if (s < end && *s == '*')
        s++;
    return trim_slice(s, (size_t)(end - s));
}



/*********************************************/
/*         PROC STATEMENT NAMES              */
/*********************************************/

/**
 * @brief Match "<IDENT> : PROC" at the start of a code line.
 *
 * Rejects identifiers that merely start with "PROC" but continue as a
 * longer word (e.g. "PROCESS").
 *
 * @param line The code line to match against.
 * @return The identifier as a slice into line's backing buffer, or
 *         { NULL, 0 } if line does not match.
 */
Line match_proc_start(Line line)
{
    Line none = { NULL, 0 };
    const char *end = line.data + line.len;
    const char *s = skip_ws_n(line.data, end);
    const char *idEnd, *p;

    if (s >= end || (!isalpha((unsigned char)*s) && *s != '_'))
        return none;
    p = s;
    while (p < end && (isalnum((unsigned char)*p) || *p == '_'))
        p++;
    idEnd = p;
    p = skip_ws_n(p, end);
    if (p >= end || *p != ':')
        return none;
    p = skip_ws_n(p + 1, end);
    if (!has_prefix_ci(p, end, "PROC"))
        return none;
    if (p + 4 < end && (isalnum((unsigned char)p[4]) || p[4] == '_'))
        return none; /* e.g. PROCESS */
    return (Line){ (char *)s, (size_t)(idEnd - s) };
}

/**
 * @brief Match "?AsaXMac ProcEntry(<IDENT>)" at the start of a code line.
 *
 * Case-insensitive. "ProcEnd" deliberately does not match this pattern.
 *
 * @param line The code line to match against.
 * @return The identifier as a slice into line's backing buffer, or
 *         { NULL, 0 } if line does not match.
 */
Line match_procentry(Line line)
{
    Line none = { NULL, 0 };
    const char *end = line.data + line.len;
    const char *s = skip_ws_n(line.data, end);
    const char *idStart, *p, *q;

    if (s >= end || *s != '?')
        return none;
    s++;
    if (!has_prefix_ci(s, end, "AsaXMac"))
        return none;
    s += 7;
    if (s >= end || !isspace((unsigned char)*s))
        return none;
    s = skip_ws_n(s, end);
    if (!has_prefix_ci(s, end, "ProcEntry"))
        return none;
    s = skip_ws_n(s + 9, end);
    if (s >= end || *s != '(')
        return none;
    idStart = skip_ws_n(s + 1, end);
    p = idStart;
    while (p < end && (isalnum((unsigned char)*p) || *p == '_'))
        p++;
    q = skip_ws_n(p, end);
    if (p == idStart || q >= end || *q != ')')
        return none;
    return (Line){ (char *)idStart, (size_t)(p - idStart) };
}



/*********************************************/
/*               SIGNATURE                   */
/*********************************************/
/**
 * @brief Accumulate one more line of a PL/X procedure signature into sig.
 *
 * Scans line and appends its content to sig (joined to any prior content
 * with a single space), tracking comment/string/paren-depth state in *st
 * across calls so the signature can span multiple source lines. Block
 * comment text is dropped from the output; quoted string content is kept
 * verbatim. Scanning stops as soon as a ';' is seen at paren depth 0
 * outside a comment or string, which marks the end of the signature.
 *
 * @param sig The signature buffer being built up across calls.
 * @param line The next line of source to consume.
 * @param st Comment/string/depth state carried across calls; updated in
 *           place.
 * @return Non-zero once the terminating ';' has been consumed (the
 *         signature is complete), 0 if more lines are needed.
 */
int sig_consume(StrBuf *sig, Line line, SigState *st)
{
    const char *p = line.data;
    const char *end = line.data + line.len;

    if (sig->len)
        sb_puts(sig, " ");
    while (p < end) {
        if (st->inComment) {
            if (p + 1 < end && p[0] == '*' && p[1] == '/') {
                st->inComment = 0;
                p += 2;
            } else {
                p++;
            }
            continue;
        }
        if (st->inString) {
            sb_putn(sig, p, 1);
            if (*p == '\'')
                st->inString = 0;
            p++;
            continue;
        }
        if (p + 1 < end && p[0] == '/' && p[1] == '*') {
            st->inComment = 1;
            p += 2;
            continue;
        }
        if (*p == '\'')
            st->inString = 1;
        else if (*p == '(')
            st->depth++;
        else if (*p == ')' && st->depth > 0)
            st->depth--;
        else if (*p == ';' && st->depth == 0) {
            sb_putn(sig, p, 1);
            return 1;
        }
        sb_putn(sig, p, 1);
        p++;
    }
    return 0;
}



/*********************************************/
/*               LABEL LOOKUP                */
/*********************************************/

/*
 * Case-insensitive label -> normalized field lookup. The convention marks
 * this list as non-exhaustive: extend it with new synonyms here without
 * touching the parse logic.
 */
const struct {
    const char *label;
    FieldId field;
} LABEL_TABLE[] = {
    { "Title",    FIELD_NAME },
    { "Routine",  FIELD_NAME },
    { "Name",     FIELD_NAME },
    { "Logic",    FIELD_DESCRIPTION },
    { "Function", FIELD_DESCRIPTION },
    { "Input",    FIELD_INPUT },
    { "Inputs",   FIELD_INPUT },
    { "Output",   FIELD_OUTPUT },
    { "Outputs",  FIELD_OUTPUT },
};

/**
 * @brief Try to parse a "<LABEL><ws>: <content>" line against LABEL_TABLE.
 *
 * @param content The line content to parse.
 * @param rest Set to the slice past the ':' on success (whenever the
 *             return value is not FIELD_NONE); left untouched when
 *             content is not label-shaped.
 * @return FIELD_NONE when content is not label-shaped (treated as a
 *         continuation of the current field), FIELD_UNKNOWN for a labeled
 *         line whose label is not in LABEL_TABLE, or the matching FieldId
 *         otherwise.
 */
FieldId parse_label(Line content, Line *rest)
{
    const char *start = content.data;
    const char *end = content.data + content.len;
    const char *p = content.data;
    size_t n, i;

    if (p >= end || !isalpha((unsigned char)*p))
        return FIELD_NONE;
    while (p < end && (isalnum((unsigned char)*p) || *p == '_' || *p == '-'))
        p++;
    n = (size_t)(p - start);
    while (p < end && (*p == ' ' || *p == '\t'))
        p++;
    if (p >= end || *p != ':')
        return FIELD_NONE;
    rest->data = (char *)(p + 1);
    rest->len = (size_t)(end - (p + 1));

    for (i = 0; i < sizeof(LABEL_TABLE) / sizeof(LABEL_TABLE[0]); i++) {
        if (
            strlen(LABEL_TABLE[i].label) == n &&
            strn_ieq(LABEL_TABLE[i].label, start, n)
        )
            return LABEL_TABLE[i].field;
    }
    return FIELD_UNKNOWN;
}



/*********************************************/
/*          COMMENT LINE HANDLING            */
/*********************************************/

/**
 * @brief Extract the inner content of a single-line comment.
 *
 * Recognizes a line whose comment opens and closes on that same line,
 * trims the interior, and strips a trailing change-activity tag such as
 * "@L0A" or "@00C" if present.
 *
 * @param line The line to inspect.
 * @return The trimmed, tag-stripped interior as a slice into line's
 *         backing buffer, or { NULL, 0 } if line is not a single-line
 *         comment.
 */
Line comment_content(Line line)
{
    const char *bufEnd = line.data + line.len;
    const char *s = skip_ws_n(line.data, bufEnd);

    if (bufEnd - s < 2 || s[0] != '/' || s[1] != '*') return (Line){ NULL, 0 };

    size_t len = (size_t)(bufEnd - s);

    while (len && isspace((unsigned char)s[len - 1]))
        len--;
    if (
        len < 4 ||
        s[len - 2] != '*' ||
        s[len - 1] != '/'
    ) return (Line){ NULL, 0 };

    Line content = trim_slice(s + 2, len - 4);

    /* Strip the trailing change-activity tag (@L0A, @00C, ...) by shortening
     * the slice - the backing buffer is read-only, so we never write to it. */
    const char *c = content.data;
    size_t n = content.len;
    if (
        n >= 4 &&
        (n == 4 || isspace((unsigned char)c[n - 5])) &&
        c[n - 4] == '@' &&
        isalnum((unsigned char)c[n - 3]) &&
        isalnum((unsigned char)c[n - 2]) &&
        isalnum((unsigned char)c[n - 1])
    ) {
        n -= 4;
        while (n && isspace((unsigned char)c[n - 1])) n--;
        content.len = n;
    }
    return content;
}

/**
 * @brief Test whether content is a divider/banner line.
 *
 * A banner line's content consists solely of '*' characters (whitespace
 * has already been trimmed out of content by the caller).
 *
 * @param content The (already comment-extracted) line content to test.
 * @return Non-zero if content is made up entirely of '*' characters
 *         (including the empty string), 0 otherwise.
 */
int is_banner(Line content)
{
    for (size_t i = 0; i < content.len; i++) {
        if (content.data[i] != '*')
            return 0;
    }
    return 1;
}



/*********************************************/
/*          DOC BLOCK ACCUMULATOR            */
/*********************************************/

/**
 * @brief Initialize a DocBlock to its empty, inactive state.
 *
 * @param b The DocBlock to initialize.
 */
void block_init(DocBlock *b)
{
    b->active = 0;
    b->closed = 0;
    b->prolog = 0;
    b->current = FIELD_NONE;
    b->startLine = 0;
    sb_init(&b->name);
    sb_init(&b->description);
    sb_init(&b->output);
    sl_init(&b->inputLines);
}

/**
 * @brief Free a DocBlock's accumulated content and reset it to empty.
 *
 * @param b The DocBlock to reset.
 */
void block_reset(DocBlock *b)
{
    sb_free(&b->name);
    sb_free(&b->description);
    sb_free(&b->output);
    sl_free(&b->inputLines);
    block_init(b);
}

/**
 * @brief Append one line of text to the given field of a DocBlock.
 *
 * Trims leading/trailing whitespace from text before appending. An empty
 * (post-trim) text still opens field as the block's current field so
 * later continuation lines are routed to it, but contributes no content
 * itself. For FIELD_INPUT inside a Method Prolog block, a continuation
 * line with no " - " separator is treated as a wrapped type and joined
 * onto the previous input line instead of starting a new one.
 *
 * @param b The DocBlock being built up.
 * @param field The field this text belongs to.
 * @param text The raw line content to append.
 */
void block_append(DocBlock *b, FieldId field, Line text)
{
    const char *tend = text.data + text.len;
    const char *t = skip_ws_n(text.data, tend);
    size_t tlen = (size_t)(tend - t);
    b->current = field;
    if (tlen == 0)
        return; /* field opened, content on the next line(s) */
    switch (field) {
    case FIELD_NAME:
        sb_join_n(&b->name, t, tlen);
        break;
    case FIELD_DESCRIPTION:
        sb_join_n(&b->description, t, tlen);
        break;
    case FIELD_OUTPUT:
        sb_join_n(&b->output, t, tlen);
        break;
    case FIELD_INPUT:
        if (b->prolog && b->inputLines.count > 0 && !contains_ci(t, tend, " - ")) {
            /* Method Prolog rows wrap the type onto a bare line with no
             * " - " separator: join it to the previous row's description. */
            char **last = &b->inputLines.items[b->inputLines.count - 1];
            size_t oldLen = strlen(*last);
            *last = xrealloc(*last, oldLen + 1 + tlen + 1);
            (*last)[oldLen] = ' ';
            memcpy(*last + oldLen + 1, t, tlen);
            (*last)[oldLen + 1 + tlen] = '\0';
        } else {
            sl_push(&b->inputLines, xstrndup(t, tlen));
        }
        break;
    default:
        break;
    }
}



/*********************************************/
/*        SYMBOL / MODULE CONSTRUCTION       */
/*********************************************/

/**
 * @brief Match a "Where <name> is:" clause line and extract the name.
 *
 * Case-insensitive; requires the line to end (after trimming trailing
 * whitespace) with "is:" preceded by "Where ".
 *
 * @param line The NUL-terminated line to match.
 * @return A newly heap-allocated copy of the trimmed name, or NULL if
 *         line does not match the "Where ... is:" shape or the name is
 *         empty.
 */
char *match_where_clause(const char *line)
{
    size_t len = strlen(line);
    char *name;

    if (!strn_ieq(line, "Where", 5) || !isspace((unsigned char)line[5]))
        return NULL;
    while (len && isspace((unsigned char)line[len - 1]))
        len--;
    if (!len || line[len - 1] != ':')
        return NULL;
    len--;
    while (len && isspace((unsigned char)line[len - 1]))
        len--;
    if (len < 3 || !strn_ieq(line + len - 2, "is", 2) ||
        !isspace((unsigned char)line[len - 3]))
        return NULL;
    len -= 2;

    if (len <= 6)
        return NULL; /* "Where is:" with no name */
    name = trim_dup(line + 6, len - 6);
    if (!name[0]) {
        free(name);
        return NULL;
    }
    return name;
}

/**
 * @brief Skip a leading enumeration marker such as "1) " or "2) ".
 *
 * @param s The line to check.
 * @return A pointer past the marker and any following whitespace if s
 *         starts with a digit sequence followed by ')'; otherwise s
 *         unchanged.
 */
const char *strip_enum_marker(const char *s)
{
    const char *p = s;

    if (!isdigit((unsigned char)*p))
        return s;
    while (isdigit((unsigned char)*p))
        p++;
    if (*p != ')')
        return s;
    return skip_ws(p + 1);
}

/**
 * @brief Append a new WhereParam to a growable array, taking ownership of
 *        name.
 *
 * Doubles the array's capacity (starting at 4) when full.
 *
 * @param arr Pointer to the growable WhereParam array.
 * @param count Pointer to the current element count; incremented on
 *              return.
 * @param cap Pointer to the current capacity; updated when the array
 *            grows.
 * @param name Parameter name; ownership passes to the new array slot.
 * @return The index of the newly added element.
 */
int where_param_add(WhereParam **arr, int *count, int *cap, char *name)
{
    if (*count == *cap) {
        *cap = *cap ? *cap * 2 : 4;
        *arr = xrealloc(*arr, (size_t)*cap * sizeof(WhereParam));
    }
    (*arr)[*count].name = name;
    sb_init(&(*arr)[*count].desc);
    return (*count)++;
}

/**
 * @brief Build Symbol input parameters from the "declared names + Where
 *        blocks" input format.
 *
 * Input format 1 (see docs/plx-doccomment-convention.md, "Input Field
 * Formats"): a comma-separated parameter name list in lines[0..firstWhere)
 * followed by "Where <name> is:" description blocks, each possibly
 * containing multiple enumerated ("1) ", "2) ", ...) or wrapped-continuation
 * items, in lines[firstWhere..count). A "Where" clause whose name was not
 * declared is still added, with a warning printed to stderr unless no
 * names were declared at all. The resulting parameters are added to sym
 * in declaration/appearance order.
 *
 * @param lines The collected input-field lines for one doc-comment block.
 * @param firstWhere Index into lines of the first "Where ... is:" line.
 * @param sym The Symbol to add the built input parameters to.
 * @param filename Source filename, used only for warning diagnostics.
 * @param lineNo Source line number, used only for warning diagnostics.
 */
void build_where_params(const StrList *lines, int firstWhere,
                               Symbol *sym, const char *filename, int lineNo)
{
    WhereParam *params = NULL;
    int paramCount = 0, paramCap = 0;
    int declared, cur = -1;
    int i, j;
    StrBuf nameList;

    /* Declared names: everything before the first Where line, comma-split. */
    sb_init(&nameList);
    for (i = 0; i < firstWhere; i++)
        sb_join(&nameList, lines->items[i]);
    if (nameList.data) {
        const char *p = nameList.data;
        while (*p) {
            const char *comma = strchr(p, ',');
            size_t n = comma ? (size_t)(comma - p) : strlen(p);
            char *name = trim_dup(p, n);
            if (name[0])
                where_param_add(&params, &paramCount, &paramCap, name);
            else
                free(name);
            p += n + (comma ? 1 : 0);
        }
    }
    sb_free(&nameList);
    declared = paramCount;

    for (i = firstWhere; i < lines->count; i++) {
        char *wname = match_where_clause(lines->items[i]);
        if (wname) {
            cur = -1;
            for (j = 0; j < paramCount; j++)
                if (str_ieq(params[j].name, wname))
                    cur = j;
            if (cur >= 0) {
                free(wname);
            } else {
                if (declared > 0)
                    fprintf(stderr,
                            "%s:%d: warning: 'Where %s is:' does not match "
                            "any declared input parameter\n",
                            filename, lineNo, wname);
                cur = where_param_add(&params, &paramCount, &paramCap, wname);
            }
        } else if (cur >= 0) {
            const char *stripped = strip_enum_marker(lines->items[i]);
            if (stripped != lines->items[i]) {
                /* enumerated item: new entry, joined with "; " */
                if (params[cur].desc.len)
                    sb_puts(&params[cur].desc, "; ");
                sb_puts(&params[cur].desc, stripped);
            } else {
                /* wrapped continuation of the current item */
                sb_join(&params[cur].desc, lines->items[i]);
            }
        }
    }

    for (i = 0; i < paramCount; i++) {
        char *desc = sb_steal(&params[i].desc);
        symbol_add_input(sym, params[i].name, desc);
        free(params[i].name);
        free(desc);
    }
    free(params);
}

/**
 * @brief Build Symbol input parameters from the collected Input field
 *        lines.
 *
 * Tries the input field formats from docs/plx-doccomment-convention.md in
 * order:
 *   1. a declared name list followed by "Where <name> is:" blocks
 *      (delegated to build_where_params())
 *   2. one "name - description" row per line
 *   3. "None" (no parameters) or otherwise free text, kept as a single
 *      unnamed entry
 *
 * @param lines The collected Input field lines for one doc-comment block.
 * @param sym The Symbol whose input array is populated.
 * @param filename Source filename, forwarded for warning diagnostics.
 * @param lineNo Source line number, forwarded for warning diagnostics.
 */
void build_input_params(const StrList *lines, Symbol *sym,
                               const char *filename, int lineNo)
{
    int i, splittable;

    sym->input = NULL;
    sym->inputCount = 0;
    if (lines->count == 0)
        return;

    /* "Input: None" means no parameters. */
    if (lines->count == 1 && str_ieq(lines->items[0], "None"))
        return;

    for (i = 0; i < lines->count; i++) {
        char *wname = match_where_clause(lines->items[i]);
        if (wname) {
            free(wname);
            build_where_params(lines, i, sym, filename, lineNo);
            symbol_shrink_inputs_to_fit(sym);
            return;
        }
    }

    splittable = 1;
    for (i = 0; i < lines->count; i++) {
        if (!strstr(lines->items[i], " - "))
            splittable = 0;
    }

    if (splittable) {
        for (i = 0; i < lines->count; i++) {
            const char *sep = strstr(lines->items[i], " - ");
            char *name = trim_dup(lines->items[i],
                                  (size_t)(sep - lines->items[i]));
            symbol_add_input(sym, name, skip_ws(sep + 3));
            free(name);
        }
    } else {
        StrBuf joined;
        char *name;
        sb_init(&joined);
        for (i = 0; i < lines->count; i++)
            sb_join(&joined, lines->items[i]);
        name = sb_steal(&joined);
        symbol_add_input(sym, name, "");
        free(name);
    }
    symbol_shrink_inputs_to_fit(sym);
}

/**
 * @brief Turn the pending doc block into a Symbol appended to mod.
 *
 * Steals the accumulated name/description/output strings out of b, strips
 * a trailing ':' or whitespace from the name, builds the input parameter
 * list via build_input_params(), and cross-checks the doc-comment name
 * against procName (warning to stderr on mismatch, or if procName carries
 * no data at all, meaning the block was never followed by a PROC
 * statement). Resets b to empty before returning.
 *
 * @param b The pending DocBlock to convert; reset to empty on return.
 * @param mod The Module to append the new Symbol to.
 * @param procName The matched procedure name, or { NULL, 0 } for an
 *                 orphan block with no following PROC statement.
 * @param signature The captured procedure signature, or NULL for an
 *                   orphan block.
 * @param lineNo The line number of the PROC statement (or of b's start,
 *               for an orphan block), used for the resulting Symbol's
 *               line and for warning diagnostics.
 */
void block_to_symbol(DocBlock *b, Module *mod, Line procName,
                     const char *signature, int lineNo)
{
    Symbol *sym = module_add_symbol(mod);
    size_t n;

    sym->name = sb_steal(&b->name);
    /* Titles often carry a trailing ':' ("Title: ADD_STUDENT:") - strip it. */
    n = strlen(sym->name);
    while (n && (sym->name[n - 1] == ':' ||
                 isspace((unsigned char)sym->name[n - 1])))
        n--;
    sym->name[n] = '\0';

    sym->signature = signature ? xstrdup(signature) : NULL;
    sym->description = sb_steal(&b->description);
    sym->output = sb_steal(&b->output);

    sym->line = (uint32_t)(procName.data ? lineNo : b->startLine);
    /* PL/X only ever documents procedures - both plain 'NAME: PROC(...)' and
     * '?AsaXMac ProcEntry(NAME)' macro routines normalize to the same kind. */
    sym->type = xstrdup("procedure");
    /* notes/diagram: no PL/X logic yet - left NULL by module_add_symbol. */

    build_input_params(&b->inputLines, sym, mod->filename, b->startLine);

    /* Cross-check doc name vs. actual procedure name (per convention). */
    if (procName.data && sym->name[0] &&
        !(strlen(sym->name) == procName.len &&
          strn_ieq(sym->name, procName.data, procName.len)))
        fprintf(stderr,
                "%s:%d: warning: doc-comment name '%s' does not match "
                "procedure name '%.*s'\n",
                mod->filename, lineNo, sym->name,
                (int)procName.len, procName.data);
    if (!procName.data)
        fprintf(stderr,
                "%s:%d: warning: doc-comment block '%s' is not followed by "
                "a PROC statement\n",
                mod->filename, b->startLine,
                sym->name[0] ? sym->name : "(unnamed)");

    block_reset(b);
}

/**
 * @brief Feed one line of doc-comment content into the pending DocBlock.
 *
 * Shared by the single-line comment path and the Method Prolog path of
 * the main parse loop in plx_parse_file(). Classifies content via
 * parse_label(): a labeled line starts or continues a field (flushing any
 * already-closed pending block to mod first); an unknown label stops
 * feeding the previous field; anything else is treated as a continuation
 * of the currently open field, if any.
 *
 * @param blk The pending DocBlock to feed.
 * @param mod The Module a flushed block (if any) is appended to as a
 *            Symbol.
 * @param content One line of doc-comment content.
 * @param lineNo The source line number, used as the new block's start
 *               line and for diagnostics.
 */
void feed_doc_line(DocBlock *blk, Module *mod, Line content,
                   int lineNo)
{
    Line rest = { NULL, 0 };
    FieldId field = parse_label(content, &rest);

    if (field == FIELD_NONE) {
        /* continuation of the currently open field */
        if (blk->active && !blk->closed &&
            blk->current != FIELD_NONE &&
            blk->current != FIELD_UNKNOWN)
            block_append(blk, blk->current, content);
    } else if (field == FIELD_UNKNOWN) {
        /* unknown label: stop feeding the previous field */
        if (blk->active && !blk->closed)
            blk->current = FIELD_UNKNOWN;
    } else {
        if (blk->closed) {
            /* new block starts while one is pending: flush it */
            block_to_symbol(blk, mod, (Line){ NULL, 0 }, NULL, lineNo);
        }
        if (!blk->active)
            blk->startLine = lineNo;
        blk->active = 1;
        block_append(blk, field, rest);
    }
}