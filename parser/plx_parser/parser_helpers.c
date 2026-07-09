#include "parser_helpers.h"
#include <ctype.h>



/*********************************************/
/*         PROC STATEMENT NAMES              */
/*********************************************/

/* Match "<IDENT> : PROC" at the start of a code line; returns the name. */
char *match_proc_start(const char *line)
{
    const char *s = skip_ws(line);
    const char *idEnd, *p;

    if (!isalpha((unsigned char)*s) && *s != '_')
        return NULL;
    p = s;
    while (isalnum((unsigned char)*p) || *p == '_')
        p++;
    idEnd = p;
    p = skip_ws(p);
    if (*p != ':')
        return NULL;
    p = skip_ws(p + 1);
    if (!strn_ieq(p, "PROC", 4))
        return NULL;
    if (isalnum((unsigned char)p[4]) || p[4] == '_')
        return NULL; /* e.g. PROCESS */
    return xstrndup(s, (size_t)(idEnd - s));
}

/*
 * Match "?AsaXMac ProcEntry(<IDENT>)" at the start of a code line; returns
 * the heap-allocated name or NULL. Case-insensitive. ProcEnd deliberately
 * does not match.
 */
char *match_procentry(const char *line)
{
    const char *s = skip_ws(line);
    const char *idStart, *p;

    if (*s != '?')
        return NULL;
    s++;
    if (!strn_ieq(s, "AsaXMac", 7))
        return NULL;
    s += 7;
    if (!isspace((unsigned char)*s))
        return NULL;
    s = skip_ws(s);
    if (!strn_ieq(s, "ProcEntry", 9))
        return NULL;
    s = skip_ws(s + 9);
    if (*s != '(')
        return NULL;
    idStart = skip_ws(s + 1);
    p = idStart;
    while (isalnum((unsigned char)*p) || *p == '_')
        p++;
    if (p == idStart || *skip_ws(p) != ')')
        return NULL;
    return xstrndup(idStart, (size_t)(p - idStart));
}



/*********************************************/
/*               SIGNATURE                   */
/*********************************************/
int sig_consume(StrBuf *sig, const char *line, SigState *st)
{
    const char *p = line;

    if (sig->len)
        sb_puts(sig, " ");
    while (*p) {
        if (st->inComment) {
            if (p[0] == '*' && p[1] == '/') {
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
        if (p[0] == '/' && p[1] == '*') {
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