// Generic helpers shared across the java_parser module: error-checked
// allocation, a growable string Buffer, and small text-scanning utilities.
// Header-only (static) so each translation unit that includes it gets its
// own private copy - nothing here is part of the public java_parser API.
#ifndef JAVA_PARSER_UTIL_H
#define JAVA_PARSER_UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

//Memory allocation with error checking wrappers. If malloc fails the program exits with an error message.
static inline void *xmalloc(size_t size) {
    void *p = malloc(size ? size : 1);
    if(!p) {
        fprintf(stderr, "zdoc: Out of memory.\n");
        exit(1);
    }
    return p;
}

static inline void *xrealloc(void *p, size_t size) {
    void *q = realloc(p, size ? size : 1);
    if(!q) {
        fprintf(stderr, "zdoc: Out of memory.\n");
        exit(1);
    }
    return q;
}

//Duplicate a string with error checking. If malloc fails the program exits with an error message.
static inline char *xstrndup(const char *s, size_t size) {
    char *p = xmalloc(size + 1);
    memcpy(p, s, size);
    p[size] = '\0';
    return p;
}

//Check if a character is a valid Java identifier character.
static inline int is_ident(char c) {
    return isalnum((unsigned char)c) || c == '$' || c == '_';
}

//A buffer for building strings.
typedef struct {
    char *data;
    size_t len;
    size_t cap;
} Buffer;

//Append a string to the buffer. The buffers memory is expanded if necessary.
static inline void buffer_put(Buffer *b, char *s, size_t size) {
    if(b->len + size >= b->cap) {
        while(b->len + size >= b->cap) b->cap = b->cap ? b->cap * 2 : 64;
        b->data = xrealloc(b->data, b->cap);
    }
    memcpy(b->data + b->len, s, size);
    b->len += size;
    b->data[b->len] = '\0';
}

//Append text to a buffer, inserting a joining separator before it if the buffer already has content.
static inline void buffer_join(Buffer *b, const char *sep, const char *s, size_t len) {
    if(b->len > 0) buffer_put(b, (char *)sep, strlen(sep));
    if(len > 0) buffer_put(b, (char *)s, len);
}

static inline size_t skip_whitespace(const char *b, size_t i, size_t len) {
    while(i < len && isspace((unsigned char)b[i])) i++;
    return i;
}

static inline size_t skip_line_comment(const char *b, size_t i, size_t len) {
    while(i < len && b[i] != '\n') i++;
    return i;
}

//Trim leading and trailing whitespace off a slice, returning the trimmed slice via *out_start/*out_len.
static inline void trim(const char *s, size_t len, const char **out_start, size_t *out_len) {
    size_t start = 0, end = len;
    while(start < end && isspace((unsigned char)s[start])) start++;
    while(end > start && isspace((unsigned char)s[end - 1])) end--;
    *out_start = s + start;
    *out_len = end - start;
}

//True if the (untrimmed) line contains only whitespace and/or '*' characters - i.e. a blank line or a divider line.
static inline int is_blank_or_divider(const char *s, size_t len) {
    for(size_t i = 0; i < len; i++)
        if(s[i] != '*' && !isspace((unsigned char)s[i])) return 0;
    return 1;
}

#endif
