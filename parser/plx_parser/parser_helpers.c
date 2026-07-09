#include "parser_helpers.h"



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