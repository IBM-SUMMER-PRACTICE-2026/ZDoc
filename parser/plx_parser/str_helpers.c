/* Small general-purpose helpers shared across the plx_parser sources. */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "str_helpers.h"

/**
 * @brief Case-insensitively compare two NUL-terminated strings for equality.
 *
 * @param a First string.
 * @param b Second string.
 * @return Non-zero if a and b are equal ignoring case, 0 otherwise.
 */
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

/**
 * @brief Case-insensitively compare the first n characters of two strings.
 *
 * Stops early and reports equality as soon as a's NUL terminator is
 * reached, without requiring b to also terminate at that point.
 *
 * @param a First string.
 * @param b Second string.
 * @param n Maximum number of characters to compare.
 * @return Non-zero if the compared characters match ignoring case, 0 on
 *         the first mismatch.
 */
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

/**
 * @brief Case-insensitively search for needle within hay.
 *
 * @param hay The NUL-terminated string to search.
 * @param needle The NUL-terminated string to search for; an empty needle
 *               matches immediately at the start of hay.
 * @return Pointer to the first matching position in hay, or NULL if
 *         needle does not occur.
 */
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

/**
 * @brief Skip leading whitespace in a NUL-terminated string.
 *
 * @param s The string to scan.
 * @return Pointer to the first non-whitespace character, or to the
 *         terminating NUL if s is all whitespace.
 */
const char *skip_ws(const char *s)
{
    while (*s && isspace((unsigned char)*s))
        s++;
    return s;
}

/**
 * @brief Skip leading whitespace in a length-bounded slice.
 *
 * Never reads at or past end, for scanning directly out of a
 * non-NUL-terminated buffer.
 *
 * @param s Start of the slice to scan.
 * @param end End of the slice (exclusive).
 * @return Pointer to the first non-whitespace character, or end if the
 *         whole slice is whitespace.
 */
const char *skip_ws_n(const char *s, const char *end)
{
    while (s < end && isspace((unsigned char)*s))
        s++;
    return s;
}

/**
 * @brief Test whether the bounded slice [s, end) begins with lit.
 *
 * Case-insensitive. Never reads at or past end. lit is an ordinary
 * NUL-terminated string literal, not buffer data.
 *
 * @param s Start of the slice to test.
 * @param end End of the slice (exclusive).
 * @param lit The literal prefix to look for.
 * @return Non-zero if [s, end) starts with lit ignoring case, 0 otherwise.
 */
int has_prefix_ci(const char *s, const char *end, const char *lit)
{
    for (; *lit; s++, lit++) {
        if (s >= end || tolower((unsigned char)*s) != tolower((unsigned char)*lit))
            return 0;
    }
    return 1;
}

/**
 * @brief Test whether needle occurs anywhere within the bounded slice
 *        [s, end).
 *
 * Case-insensitive. Never reads at or past end. needle is an ordinary
 * NUL-terminated string literal, not buffer data.
 *
 * @param s Start of the slice to search.
 * @param end End of the slice (exclusive).
 * @param needle The literal substring to look for; an empty needle always
 *               matches.
 * @return Non-zero if needle occurs within [s, end) ignoring case, 0
 *         otherwise.
 */
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

/**
 * @brief Duplicate s[0..n) with leading and trailing whitespace trimmed.
 *
 * @param s Start of the slice to trim and duplicate.
 * @param n Length of the slice.
 * @return A newly heap-allocated, NUL-terminated copy of the trimmed
 *         content.
 */
char *trim_dup(const char *s, size_t n)
{
    const char *end = s + n;
    while (s < end && isspace((unsigned char)*s))
        s++;
    while (end > s && isspace((unsigned char)end[-1]))
        end--;
    return xstrndup(s, (size_t)(end - s));
}

/**
 * @brief Trim leading and trailing whitespace from s[0..n) without copying.
 *
 * Like trim_dup() but returns a slice into the original s[0..n); the
 * result stays valid only as long as s does.
 *
 * @param s Start of the slice to trim.
 * @param n Length of the slice.
 * @return The trimmed content as a Line slice into s.
 */
Line trim_slice(const char *s, size_t n)
{
    const char *end = s + n;
    while (s < end && isspace((unsigned char)*s))
        s++;
    while (end > s && isspace((unsigned char)end[-1]))
        end--;
    return (Line){ (char *)s, (size_t)(end - s) };
}

/**
 * @brief Collapse runs of whitespace to single spaces, in place.
 *
 * Also drops any leading whitespace entirely.
 *
 * @param s The NUL-terminated string to rewrite in place.
 * @return s, for convenient chaining.
 */
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

/**
 * @brief Initialize an empty StrBuf.
 *
 * @param b The StrBuf to initialize.
 */
void sb_init(StrBuf *b)
{
    b->data = NULL;
    b->len = b->cap = 0;
}

/**
 * @brief Append n raw bytes to a StrBuf, growing it as needed.
 *
 * Doubles the buffer's capacity as needed and keeps the buffer
 * NUL-terminated after the append.
 *
 * @param b The StrBuf to append to.
 * @param s Bytes to append.
 * @param n Number of bytes to append.
 */
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

/**
 * @brief Append a NUL-terminated string to a StrBuf.
 *
 * @param b The StrBuf to append to.
 * @param s The NUL-terminated string to append.
 */
void sb_puts(StrBuf *b, const char *s)
{
    sb_putn(b, s, strlen(s));
}

/**
 * @brief Append s to a StrBuf, separated from existing content by a space.
 *
 * A single space is inserted before s only when b already holds content.
 * Does nothing if s is the empty string.
 *
 * @param b The StrBuf to append to.
 * @param s The NUL-terminated string to append.
 */
void sb_join(StrBuf *b, const char *s)
{
    if (*s == '\0')
        return;
    if (b->len)
        sb_puts(b, " ");
    sb_puts(b, s);
}

/**
 * @brief Like sb_join(), but for a length-delimited (non-NUL-terminated)
 *        slice.
 *
 * Does nothing if n is 0.
 *
 * @param b The StrBuf to append to.
 * @param s Start of the slice to append.
 * @param n Length of the slice.
 */
void sb_join_n(StrBuf *b, const char *s, size_t n)
{
    if (n == 0)
        return;
    if (b->len)
        sb_puts(b, " ");
    sb_putn(b, s, n);
}

/**
 * @brief Hand the accumulated string to the caller and reset the buffer.
 *
 * @param b The StrBuf to steal from; reset to empty on return.
 * @return A newly heap-allocated, NUL-terminated copy of the buffer's
 *         content, shrunk to its exact size; an allocated empty string if
 *         b held no content.
 */
char *sb_steal(StrBuf *b)
{
    char *s = b->data ? xrealloc(b->data, b->len + 1) : xstrdup("");
    sb_init(b);
    return s;
}

/**
 * @brief Free a StrBuf's storage and reset it to empty.
 *
 * @param b The StrBuf to free.
 */
void sb_free(StrBuf *b)
{
    free(b->data);
    sb_init(b);
}

/**
 * @brief Initialize an empty StrList.
 *
 * @param l The StrList to initialize.
 */
void sl_init(StrList *l)
{
    l->items = NULL;
    l->count = l->cap = 0;
}

/**
 * @brief Append a heap-allocated string to a StrList, taking ownership.
 *
 * Doubles the list's capacity (starting at 8) when full.
 *
 * @param l The StrList to append to.
 * @param s The string to append; ownership passes to the list.
 */
void sl_push(StrList *l, char *s)
{
    if (l->count == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 8;
        l->items = xrealloc(l->items, (size_t)l->cap * sizeof(char *));
    }
    l->items[l->count++] = s;
}

/**
 * @brief Free every string in a StrList plus its backing array.
 *
 * Resets l to empty afterward.
 *
 * @param l The StrList to free.
 */
void sl_free(StrList *l)
{
    int i;
    for (i = 0; i < l->count; i++)
        free(l->items[i]);
    free(l->items);
    sl_init(l);
}
