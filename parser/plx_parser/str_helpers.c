/* Small general-purpose helpers shared across the plx_parser sources. */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "str_helpers.h"

int str_ieq(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return 0;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

int strn_ieq(const char *a, const char *b, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) {
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i]))
            return 0;
        if (a[i] == '\0')
            return 1;
    }
    return 1;
}

const char *str_istr(const char *hay, const char *needle)
{
    size_t i;

    if (!*needle)
        return hay;
    for (; *hay; hay++) {
        for (i = 0; needle[i]; i++) {
            if (tolower((unsigned char)hay[i]) !=
                tolower((unsigned char)needle[i]))
                break;
        }
        if (!needle[i])
            return hay;
    }
    return NULL;
}

const char *skip_ws(const char *s)
{
    while (*s && isspace((unsigned char)*s))
        s++;
    return s;
}

const char *skip_ws_n(const char *s, const char *end)
{
    while (s < end && isspace((unsigned char)*s))
        s++;
    return s;
}

int has_prefix_ci(const char *s, const char *end, const char *lit)
{
    for (; *lit; s++, lit++) {
        if (s >= end || tolower((unsigned char)*s) != tolower((unsigned char)*lit))
            return 0;
    }
    return 1;
}

int contains_ci(const char *s, const char *end, const char *needle)
{
    if (!*needle)
        return 1;
    for (; s < end; s++) {
        const char *a = s, *b = needle;
        while (a < end && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) {
            a++;
            b++;
        }
        if (!*b)
            return 1;
    }
    return 0;
}

char *trim_dup(const char *s, size_t n)
{
    const char *end = s + n;
    while (s < end && isspace((unsigned char)*s))
        s++;
    while (end > s && isspace((unsigned char)end[-1]))
        end--;
    return xstrndup(s, (size_t)(end - s));
}

Line trim_slice(const char *s, size_t n)
{
    const char *end = s + n;
    while (s < end && isspace((unsigned char)*s))
        s++;
    while (end > s && isspace((unsigned char)end[-1]))
        end--;
    return (Line){ (char *)s, (size_t)(end - s) };
}

char *squeeze_ws(char *s)
{
    char *r = s, *w = s;
    int space = 0;
    while (*r && isspace((unsigned char)*r))
        r++;
    for (; *r; r++) {
        if (isspace((unsigned char)*r)) {
            space = 1;
        } else {
            if (space && w != s)
                *w++ = ' ';
            space = 0;
            *w++ = *r;
        }
    }
    *w = '\0';
    return s;
}

void sb_init(StrBuf *b)
{
    b->data = NULL;
    b->len = b->cap = 0;
}

void sb_putn(StrBuf *b, const char *s, size_t n)
{
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap * 2 : 64;
        while (cap < b->len + n + 1)
            cap *= 2;
        b->data = xrealloc(b->data, cap);
        b->cap = cap;
    }
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
}

void sb_puts(StrBuf *b, const char *s)
{
    sb_putn(b, s, strlen(s));
}

void sb_join(StrBuf *b, const char *s)
{
    if (*s == '\0')
        return;
    if (b->len)
        sb_puts(b, " ");
    sb_puts(b, s);
}

void sb_join_n(StrBuf *b, const char *s, size_t n)
{
    if (n == 0)
        return;
    if (b->len)
        sb_puts(b, " ");
    sb_putn(b, s, n);
}

char *sb_steal(StrBuf *b)
{
    char *s = b->data ? xrealloc(b->data, b->len + 1) : xstrdup("");
    sb_init(b);
    return s;
}

void sb_free(StrBuf *b)
{
    free(b->data);
    sb_init(b);
}

void sl_init(StrList *l)
{
    l->items = NULL;
    l->count = l->cap = 0;
}

void sl_push(StrList *l, char *s)
{
    if (l->count == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 8;
        l->items = xrealloc(l->items, (size_t)l->cap * sizeof(char *));
    }
    l->items[l->count++] = s;
}

void sl_free(StrList *l)
{
    int i;
    for (i = 0; i < l->count; i++)
        free(l->items[i]);
    free(l->items);
    sl_init(l);
}
