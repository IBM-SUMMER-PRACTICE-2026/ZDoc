#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <strings.h> //strncasecmp (POSIX but on MSVC its _strnicmp)
#include <ctype.h>

#include "java_parser.h"


//Memory allocation with error checking wrappers. If malloc fails the program exits with an error message.
static void *xmalloc(size_t size) {
    void *p = malloc(size ? size : 1);
    if(!p) {
        fprintf(stderr, "zdoc: Out of memory.\n");
        exit(1);
    }
    return p;
}

static void *xrealloc(void *p, size_t size) {
    void *q = realloc(p, size ? size : 1);
    if(!q) {
        fprintf(stderr, "zdoc: Out of memory.\n");
        exit(1);
    }
    return q;
}


//Duplicate a string with error checking. If malloc fails the program exits with an error message.
static char *xstrndup(const char *s, size_t size) {
    char *p = xmalloc(size + 1);
    memcpy(p, s, size);
    p[size] = '\0';
    return p;
}

//Check if a character is a valid Java identifier character.
static int is_ident(char c){
    return isalnum((unsigned char)c) || c == '$' || c == '_';
}

//A buffer for building strings. It is used to build the signature of a method or constructor.
typedef struct {
    char *data;
    size_t len;
    size_t cap;
}Buffer;

//Append a string to the buffer. The buffers memory is expanded if necessary.
static void buffer_put(Buffer *b, char *s, size_t size) {
    if(b->len + size >= b->cap) {
        while(b->len + size >= b->cap) 
            b->cap = b->cap ? b->cap * 2 : 64;
        b->data = xrealloc(b->data, b->cap);
    }
    memcpy(b->data + b->len, s, size);
    b->len += size;
    b->data[b->len] = '\0';
}



void module_free(Module *m) {
    for(size_t s = 0; s < m->count; s++) {
        Symbol *y = &m->symbols[s];
        free(y->name);
        free(y->signature);
        free(y->raw_comment);
        free(y->brief);
        free(y->returns);
        free(y->diagram);
        for(size_t k = 0; k < y->param_count; k++) {
            free(y->params[k].name);
            free(y->params[k].description);
        }
        free(y->params); 
    }

    free(m->symbols);
    free(m->filename);
    m->symbols = NULL;
    m->count = m->cap = 0;
    m->filename = NULL;
}