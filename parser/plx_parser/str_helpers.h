#ifndef PLX_STR_HELPERS_H
#define PLX_STR_HELPERS_H

#include <stddef.h>
#include "../shared/parser_shared.h"

/* Case-insensitive string comparison (full / first n chars). */
int str_ieq(const char *a, const char *b);
int strn_ieq(const char *a, const char *b, size_t n);

/* Case-insensitive substring search; returns the match or NULL. */
const char *str_istr(const char *hay, const char *needle);

const char *skip_ws(const char *s);

/* Length-bounded scanners for parsing directly out of the (non-NUL-terminated)
 * file buffer: they never read at or past `end`. The `lit`/`needle` arguments
 * are ordinary NUL-terminated string literals, not buffer data. */
const char *skip_ws_n(const char *s, const char *end);
int has_prefix_ci(const char *s, const char *end, const char *lit);   /* does [s,end) begin with lit? */
int contains_ci(const char *s, const char *end, const char *needle);  /* does needle occur within [s,end)? */

/* Duplicate s[0..n) with both ends trimmed. */
char *trim_dup(const char *s, size_t n);

/* Collapse whitespace runs to single spaces, in place. */
char *squeeze_ws(char *s);

/* Growable string buffer. */
typedef struct {
    char *data;
    size_t len, cap;
} StrBuf;

void sb_init(StrBuf *b);
void sb_putn(StrBuf *b, const char *s, size_t n);
void sb_puts(StrBuf *b, const char *s);
/* Append with a single space separator when the buffer already has text. */
void sb_join(StrBuf *b, const char *s);
/* Hand the accumulated string to the caller and reset the buffer. */
char *sb_steal(StrBuf *b);
void sb_free(StrBuf *b);

/* Growable list of strings (owned). */
typedef struct {
    char **items;
    int count, cap;
} StrList;

void sl_init(StrList *l);
void sl_push(StrList *l, char *s);
void sl_free(StrList *l);

#endif
