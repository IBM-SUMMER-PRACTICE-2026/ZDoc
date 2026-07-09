#ifndef PLX_HELPERS_H
#define PLX_HELPERS_H

#include <stddef.h>
#include "../shared/parser_shared.h"

/* Allocation wrappers that abort on out-of-memory (defined in parser_shared_interface.c). */
char *xstrndup(const char *s, size_t n);

/* Case-insensitive string comparison (full / first n chars). */
int str_ieq(const char *a, const char *b);
int strn_ieq(const char *a, const char *b, size_t n);

/* Case-insensitive substring search; returns the match or NULL. */
const char *str_istr(const char *hay, const char *needle);

const char *skip_ws(const char *s);

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


// Output
const char *or_null(const char *s);

#endif
