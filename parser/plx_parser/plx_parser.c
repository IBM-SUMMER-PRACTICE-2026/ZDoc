/*
 * plx_parser — PL/X doc-comment block parser.
 *
 * Parses the "Label: content  @TAG" doc-comment blocks that precede
 * procedures, as specified in docs/plx-doccomment-convention.md, and prints
 * the extracted symbols to stdout.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "str_helpers.h"
#include "plx_parser.h"
#include "parser_helpers.h"

/*
 * Case-insensitive label -> normalized field lookup. The convention marks
 * this list as non-exhaustive: extend it with new synonyms here without
 * touching the parse logic.
 */
static const struct {
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

/*
 * Try to parse "<LABEL><ws>: <content>". Returns FIELD_NONE when the line is
 * not label-shaped (treated as a continuation), FIELD_UNKNOWN for a labeled
 * line whose label is not in the table. On success *rest points past the ':'.
 */
static FieldId parse_label(const char *content, const char **rest)
{
    const char *start = content;
    const char *p = content;
    size_t n, i;

    if (!isalpha((unsigned char)*p))
        return FIELD_NONE;
    while (isalnum((unsigned char)*p) || *p == '_' || *p == '-')
        p++;
    n = (size_t)(p - start);
    while (*p == ' ' || *p == '\t')
        p++;
    if (*p != ':')
        return FIELD_NONE;
    *rest = p + 1;

    for (i = 0; i < sizeof(LABEL_TABLE) / sizeof(LABEL_TABLE[0]); i++) {
        if (
            strlen(LABEL_TABLE[i].label) == n &&
            strn_ieq(LABEL_TABLE[i].label, start, n)
        )
            return LABEL_TABLE[i].field;
    }
    return FIELD_UNKNOWN;
}

/* ------------------------------------------------------------------ */
/* Comment line handling                                               */
/* ------------------------------------------------------------------ */

/*
 * If the line is a single-line comment (opens and closes on this line),
 * return its inner content (heap-allocated, trimmed, trailing @TAG
 * stripped). Otherwise return NULL.
 */
static char *comment_content(const char *line)
{
    const char *s = skip_ws(line);
    
    if(s[0] != '/' || s[1] != '*') return NULL;

    size_t len = strlen(s);

    char *content;
    size_t n;

    while (len && isspace((unsigned char)s[len - 1]))
        len--;
    if (
        len < 4 ||
        s[len - 2] != '*' ||
        s[len - 1] != '/'
    ) return NULL;

    content = trim_dup(s + 2, len - 4);

    /* Strip the trailing change-activity tag (@L0A, @00C, ...). */
    n = strlen(content);
    if (
        (n == 4 || isspace((unsigned char)content[n - 5])) &&
        n >= 4 && content[n - 4] == '@' &&
        isalnum((unsigned char)content[n - 3]) &&
        isalnum((unsigned char)content[n - 2]) &&
        isalnum((unsigned char)content[n - 1])
    ) {
        n -= 4;
        while (n && isspace((unsigned char)content[n - 1])) n--;
        content[n] = '\0';
    }
    return content;
}

/* A divider/banner line: content made of '*' only (plus whitespace). */
static int is_banner(const char *content)
{
    for (; *content; content++) {
        if (*content != '*')
            return 0;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Doc block accumulator                                               */
/* ------------------------------------------------------------------ */

static void block_init(DocBlock *b)
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

static void block_reset(DocBlock *b)
{
    sb_free(&b->name);
    sb_free(&b->description);
    sb_free(&b->output);
    sl_free(&b->inputLines);
    block_init(b);
}

static void block_append(DocBlock *b, FieldId field, const char *text)
{
    text = skip_ws(text);
    b->current = field;
    if (*text == '\0')
        return; /* field opened, content on the next line(s) */
    switch (field) {
    case FIELD_NAME:
        sb_join(&b->name, text);
        break;
    case FIELD_DESCRIPTION:
        sb_join(&b->description, text);
        break;
    case FIELD_OUTPUT:
        sb_join(&b->output, text);
        break;
    case FIELD_INPUT:
        if (b->prolog && b->inputLines.count > 0 && !strstr(text, " - ")) {
            /* Method Prolog rows wrap the type onto a bare line with no
             * " - " separator: join it to the previous row's description. */
            char **last = &b->inputLines.items[b->inputLines.count - 1];
            size_t oldLen = strlen(*last), addLen = strlen(text);
            *last = xrealloc(*last, oldLen + 1 + addLen + 1);
            (*last)[oldLen] = ' ';
            memcpy(*last + oldLen + 1, text, addLen + 1);
        } else {
            sl_push(&b->inputLines, xstrdup(text));
        }
        break;
    default:
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Symbol / module construction                                        */
/* ------------------------------------------------------------------ */

/*
 * If the line is a "Where <name> is:" clause, return the heap-allocated
 * parameter name; otherwise return NULL. Case-insensitive.
 */
static char *match_where_clause(const char *line)
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

/* Skip an enumeration marker ("1) ", "2) ", ...) if the line starts with
 * one; returns the original pointer otherwise. */
static const char *strip_enum_marker(const char *s)
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

/* Parameter under construction for the "Where <name> is:" input format. */
typedef struct {
    char *name;
    StrBuf desc;
} WhereParam;

static int where_param_add(WhereParam **arr, int *count, int *cap, char *name)
{
    if (*count == *cap) {
        *cap = *cap ? *cap * 2 : 4;
        *arr = xrealloc(*arr, (size_t)*cap * sizeof(WhereParam));
    }
    (*arr)[*count].name = name;
    sb_init(&(*arr)[*count].desc);
    return (*count)++;
}

/*
 * Input format 1: a comma-separated parameter name list followed by
 * "Where <name> is:" description blocks with enumerated items (see
 * docs/plx-doccomment-convention.md, "Input Field Formats").
 */
static void build_where_params(const StrList *lines, int firstWhere,
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

/*
 * Build InputParam entries from the collected input lines, trying the input
 * field formats from docs/plx-doccomment-convention.md in order:
 *   1. name list + "Where <name> is:" blocks
 *   2. one "name - description" row per line
 *   3. "None" (no parameters) or free text kept as a single entry
 */
static void build_input_params(const StrList *lines, Symbol *sym,
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
}

/*
 * Turn the pending doc block into a Symbol. signature may be NULL for an
 * orphan block that was never followed by a PROC statement. procName is
 * still used here to cross-check against the doc-comment name and is not
 * stored on the resulting Symbol.
 */
static void block_to_symbol(DocBlock *b, Module *mod, const char *procName,
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
    
    sym->line = (uint32_t)(procName ? lineNo : b->startLine);
    /* PL/X only ever documents procedures - both plain 'NAME: PROC(...)' and
     * '?AsaXMac ProcEntry(NAME)' macro routines normalize to the same kind. */
    sym->type = xstrdup("procedure");
    /* notes/diagram: no PL/X logic yet - left NULL by module_add_symbol. */

    build_input_params(&b->inputLines, sym, mod->filename, b->startLine);

    /* Cross-check doc name vs. actual procedure name (per convention). */
    if (procName && sym->name[0] && !str_ieq(sym->name, procName))
        fprintf(stderr,
                "%s:%d: warning: doc-comment name '%s' does not match "
                "procedure name '%s'\n",
                mod->filename, lineNo, sym->name, procName);
    if (!procName)
        fprintf(stderr,
                "%s:%d: warning: doc-comment block '%s' is not followed by "
                "a PROC statement\n",
                mod->filename, b->startLine,
                sym->name[0] ? sym->name : "(unnamed)");

    block_reset(b);
}

/*
 * Feed one line of doc-comment content (label, continuation or unknown)
 * into the pending block. Shared by the single-line comment path and the
 * Method Prolog path of the main parse loop.
 */
static void feed_doc_line(DocBlock *blk, Module *mod, const char *content,
                          int lineNo)
{
    const char *rest = NULL;
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
            block_to_symbol(blk, mod, NULL, NULL, lineNo);
        }
        if (!blk->active)
            blk->startLine = lineNo;
        blk->active = 1;
        block_append(blk, field, rest);
    }
}


/* ------------------------------------------------------------------ */
/* File parsing                                                        */
/* ------------------------------------------------------------------ */

Module *plx_parse_file(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        perror(path);
        return NULL;
    }

    Module *mod = init_module(path);
    DocBlock blk;
    char line[MAX_LINE];
    int lineNo = 0;

    int capturing = 0;
    int inProlog = 0;
    SigState sigState = { 0, 0, 0 };
    StrBuf sig;
    char *sigProc = NULL;
    int procLine = 0; /* line the PROC/ProcEntry was matched on */

    block_init(&blk);
    sb_init(&sig);

    // Iterate line by line
    while (fgets(line, sizeof line, f)) {
        char *content;
        char *procName;

        lineNo++;

        if (capturing) {
            if (sig_consume(&sig, line, &sigState)) {
                char *sigText = squeeze_ws(sb_steal(&sig));
                if (blk.active)
                    block_to_symbol(&blk, mod, sigProc, sigText, procLine);
                else
                    fprintf(stderr,
                            "%s:%d: note: procedure '%s' has no doc-comment "
                            "block - skipped\n",
                            path, procLine, sigProc);
                free(sigText);
                free(sigProc);
                sigProc = NULL;
                capturing = 0;
            }
            continue;
        }

        // Inside a Method Prolog block (.plxmac)
        if (inProlog) {
            if (is_prolog_end(line)) {
                if (blk.active)
                    blk.closed = 1; /* block done; wait for the ProcEntry */
                inProlog = 0;
            } else {
                content = prolog_content(line);
                if (*content != '\0') /* padding-only lines are skipped */
                    feed_doc_line(&blk, mod, content, lineNo);
                free(content);
            }
            continue;
        }

        if (is_prolog_start(line)) {
            if (blk.closed) {
                /* new block starts while one is pending: flush it */
                block_to_symbol(&blk, mod, NULL, NULL, lineNo);
            }
            inProlog = 1;
            blk.prolog = 1;
            continue;
        }

        content = comment_content(line);
        if (content) {
            // Padding only
            if (*content == '\0') {
                /* padding-only line: skip, keep current field */
            }
            // Banner line
            else if (is_banner(content)) {
                if (blk.active)
                    blk.closed = 1; /* block done; wait for the PROC */
            }
            // Doc comment line
            else {
                feed_doc_line(&blk, mod, content, lineNo);
            }
            free(content);
            continue;
        }

        procName = match_proc_start(line);
        if (!procName)
            procName = match_procentry(line);
        if (procName) {
            capturing = 1;
            sigState.depth = 0;
            sigState.inComment = 0;
            sigState.inString = 0;
            sigProc = procName;
            procLine = lineNo;
            if (sig_consume(&sig, line, &sigState)) {
                char *sigText = squeeze_ws(sb_steal(&sig));
                if (blk.active)
                    block_to_symbol(&blk, mod, sigProc, sigText, procLine);
                else
                    fprintf(stderr,
                            "%s:%d: note: procedure '%s' has no doc-comment "
                            "block - skipped\n",
                            path, procLine, sigProc);
                free(sigText);
                free(sigProc);
                sigProc = NULL;
                capturing = 0;
            }
        }
        /* all other code lines are irrelevant to doc extraction */
    }

    if (blk.active)
        block_to_symbol(&blk, mod, NULL, NULL, lineNo);
    if (capturing) {
        free(sigProc);
        sb_free(&sig);
    }
    module_shrink_to_fit(mod);

    fclose(f);
    return mod;
}