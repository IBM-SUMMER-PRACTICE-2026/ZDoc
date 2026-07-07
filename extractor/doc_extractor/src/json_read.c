#include "json_read.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *xmalloc(size_t n) {
    void *p = malloc(n ? n : 1);
    if(!p) {
        fprintf(stderr, "zdoc-doc-extractor: out of memory\n");
        exit(1);
    }
    return p;
}

void *xrealloc(void *p, size_t n) {
    void *q = realloc(p, n ? n : 1);
    if(!q) {
        fprintf(stderr, "zdoc-doc-extractor: out of memory\n");
        exit(1);
    }
    return q;
}

void jskip_ws(JParser *j) {
    while(j->p < j->end && isspace((unsigned char)*j->p)) j->p++;
}

int jpeek(JParser *j) {
    jskip_ws(j);
    return j->p < j->end ? (unsigned char)*j->p : -1;
}

int jeat(JParser *j, char c) {
    jskip_ws(j);
    if(j->p >= j->end || *j->p != c) { j->ok = 0; return 0; }
    j->p++;
    return 1;
}

static int hex_digit(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

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
                    decoded = (char)(cp & 0xFF);
                    break;
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

    if(j->p >= j->end) { j->ok = 0; free(buf); return NULL; } /* unterminated */
    j->p++; /* closing quote */
    buf[len] = '\0';
    return buf;
}

long jparse_int(JParser *j) {
    jskip_ws(j);
    const char *start = j->p;
    if(j->p < j->end && (*j->p == '-' || *j->p == '+')) j->p++;
    if(j->p >= j->end || !isdigit((unsigned char)*j->p)) { j->ok = 0; return 0; }
    while(j->p < j->end && isdigit((unsigned char)*j->p)) j->p++;
    return strtol(start, NULL, 10);
}

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
