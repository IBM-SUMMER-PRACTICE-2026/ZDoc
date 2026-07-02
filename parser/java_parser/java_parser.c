#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#include "java_parser.h"





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
            free(y->params[k].descrtiption);
        }
        free(y->params); 
    }

    free(m->symbols);
    free(m->filename);
    m->symbols = NULL;
    m->count = m->cap = 0;
    m->filename = NULL;
}