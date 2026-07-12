#include "xalloc.h"

#include <stdio.h>
#include <stdlib.h>

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
