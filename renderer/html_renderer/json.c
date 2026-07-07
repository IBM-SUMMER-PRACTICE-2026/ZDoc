/*
 * Generic JSON reading primitives - no knowledge of html_renderer's own
 * schema lives here (that's html_model.c). Kept separate so this file could
 * be reused as-is if another ZDoc component ever needs to read JSON too.
 */
#include "json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//Memory allocation helpers that exit the process on OOM, same convention as
void *xmalloc(size_t n) {
    void *p = malloc(n ? n : 1);
    if(!p) {
        fprintf(stderr, "zdoc-html-renderer: out of memory\n");
        exit(1);
    }
    return p;
}

//Memory reallocation helper that exits the process on OOM, same convention as
void *xrealloc(void *p, size_t n) {
    void *q = realloc(p, n ? n : 1);
    if(!q) {
        fprintf(stderr, "zdoc-html-renderer: out of memory\n");
        exit(1);
    }
    return q;
}

//Skips whitespace (space, tab, CR, LF) until the next significant character. JSON has no comments, so there is nothing else to skip.
void jskip_ws(JParser *j) {
    while(j->p < j->end && isspace((unsigned char)*j->p)) j->p++;
}
//Peeks at the next non-whitespace character, or -1 if at the end of input.
int jpeek(JParser *j) {
    jskip_ws(j);
    return j->p < j->end ? (unsigned char)*j->p : -1;
}
//Eats the next non-whitespace character if it matches c, returning 1 on success
int jeat(JParser *j, char c) {
    jskip_ws(j);
    if(j->p >= j->end || *j->p != c) { j->ok = 0; return 0; }
    j->p++;
    return 1;
}

/* Encode a Unicode code point as UTF-8 into buf, returning the byte count.
  No surrogate-pair handling - each \uXXXX is decoded independently, which
  is sufficient for the escapes this project's own JSON emitters produce
  (plain ASCII text plus \u00XX only for control characters). */
static int utf8_encode(unsigned int cp, char *buf) {
    if(cp <= 0x7F) { buf[0] = (char)cp; return 1; }
    if(cp <= 0x7FF) {
        buf[0] = (char)(0xC0 | (cp >> 6));
        buf[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    buf[0] = (char)(0xE0 | (cp >> 12));
    buf[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    buf[2] = (char)(0x80 | (cp & 0x3F));
    return 3;
}
//Hex digit to integer, or -1 if not a hex digit. Used for \uXXXX decoding.
static int hex_digit(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

//Parse a JSON string literal (the opening quote must be the current char).
char *jparse_string(JParser *j) {
    if(!jeat(j, '"')) return NULL;

    char *buf = xmalloc(1);
    size_t len = 0, cap = 1;

    while(j->p < j->end && *j->p != '"') {
        char c = *j->p;
        if(c == '\\') {
            j->p++;
            if(j->p >= j->end) { j->ok = 0; free(buf); return NULL; }
            char esc = *j->p++;
            char decoded;
            switch(esc) {
                case '"':  decoded = '"';  break;
                case '\\': decoded = '\\'; break;
                case '/':  decoded = '/';  break;
                case 'b':  decoded = '\b'; break;
                case 'f':  decoded = '\f'; break;
                case 'n':  decoded = '\n'; break;
                case 'r':  decoded = '\r'; break;
                case 't':  decoded = '\t'; break;
                case 'u': {
                    if(j->end - j->p < 4) { j->ok = 0; free(buf); return NULL; }
                    unsigned int cp = 0;
                    for(int k = 0; k < 4; k++) {
                        int d = hex_digit(j->p[k]);
                        if(d < 0) { j->ok = 0; free(buf); return NULL; }
                        cp = (cp << 4) | (unsigned)d;
                    }
                    j->p += 4;
                    char utf8[3];
                    int n = utf8_encode(cp, utf8);
                    if(len + (size_t)n >= cap) {
                        cap = (len + (size_t)n + 1) * 2;
                        buf = xrealloc(buf, cap);
                    }
                    memcpy(buf + len, utf8, (size_t)n);
                    len += (size_t)n;
                    continue;
                }
                default: j->ok = 0; free(buf); return NULL;
            }
            if(len + 1 >= cap) { cap *= 2; buf = xrealloc(buf, cap); }
            buf[len++] = decoded;
        } else {
            if(len + 1 >= cap) { cap *= 2; buf = xrealloc(buf, cap); }
            buf[len++] = c;
            j->p++;
        }
    }

    if(j->p >= j->end) { j->ok = 0; free(buf); return NULL; } //unterminated 
    j->p++; //closing quote 
    buf[len] = '\0';
    return buf;
}

/* Every numeric field in html_model.c's schema (line, parent_index,
  parent_dir_index) is always a plain integer, so this deliberately does
  not accept fractional or exponent parts - that would be malformed input
  for this schema, not a value to tolerate. */
long jparse_int(JParser *j) {
    jskip_ws(j);
    const char *start = j->p;
    if(j->p < j->end && (*j->p == '-' || *j->p == '+')) j->p++;
    if(j->p >= j->end || !isdigit((unsigned char)*j->p)) { j->ok = 0; return 0; }
    while(j->p < j->end && isdigit((unsigned char)*j->p)) j->p++;
    return strtol(start, NULL, 10);
}

//Skips over any single JSON value and is used to ignore keys/entries the caller
void jskip_value(JParser *j) {
    int c = jpeek(j);
    if(c == '"') { char *s = jparse_string(j); free(s); return; }
    if(c == '{') {
        jeat(j, '{');
        if(jpeek(j) == '}') { jeat(j, '}'); return; }
        for(;;) {
            char *key = jparse_string(j);
            free(key);
            if(!j->ok) return;
            jeat(j, ':');
            jskip_value(j);
            if(jpeek(j) == ',') { jeat(j, ','); continue; }
            break;
        }
        jeat(j, '}');
        return;
    }
    if(c == '[') {
        jeat(j, '[');
        if(jpeek(j) == ']') { jeat(j, ']'); return; }
        for(;;) {
            jskip_value(j);
            if(jpeek(j) == ',') { jeat(j, ','); continue; }
            break;
        }
        jeat(j, ']');
        return;
    }
    if(c == 't') { j->p += 4; return; }         /* true */
    if(c == 'f') { j->p += 5; return; }         /* false */
    if(c == 'n') { j->p += 4; return; }         /* null */
    jparse_int(j);
}

/*Parses "{ "key": <value>, ... }", calling field(j, key, ctx) for each key - the callback is 
 responsible for consuming that key's value (via a jparse_* call, a nested parse_object, or jskip_
 value for anything it doesn't recognize).*/
int parse_object(JParser *j, ObjectFieldFn field, void *ctx) {
    if(!jeat(j, '{')) return 0;
    if(jpeek(j) == '}') return jeat(j, '}');
    for(;;) {
        char *key = jparse_string(j);
        if(!j->ok) { free(key); return 0; }
        jeat(j, ':');
        if(j->ok) field(j, key, ctx);
        free(key);
        if(!j->ok) return 0;
        if(jpeek(j) == ',') { jeat(j, ','); continue; }
        break;
    }
    return jeat(j, '}');
}
