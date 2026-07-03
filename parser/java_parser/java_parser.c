#include <stdlib.h>
#include <string.h>

#include "java_parser.h"
#include "util.h"
#include "doc_comment.h"

void module_free(Module *m) {
    for(size_t s = 0; s < m->count; s++) {
        Symbol *y = &m->symbols[s];
        free(y->name);
        free(y->signature);
        free(y->raw_comment);
        free(y->brief);
        free(y->returns);
        free(y->diagram);
        free(y->notes);
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

//Append a symbol to the module's symbol array. The array's memory is expanded if necessary.
static void module_push(Module *m, Symbol sym) {
    if(m->count == m->cap) {
        m->cap = m->cap ? m->cap * 2 : 8;
        m->symbols = xrealloc(m->symbols, m->cap * sizeof(Symbol));
    }
    m->symbols[m->count++] = sym;
}

//Take the declaration text right after a doc comment, up to (not including) the first '{' or ';',
//and collapse all whitespace runs to single spaces. Returns NULL if there was nothing there.
static char *extract_signature(const char *src, size_t start, size_t len) {
    Buffer b = {0};
    int pending_space = 0;

    size_t i = start;
    while(i < len && src[i] != '{' && src[i] != ';') {
        if(isspace((unsigned char)src[i])) {
            if(b.len > 0) pending_space = 1;
        } else {
            if(pending_space) buffer_put(&b, " ", 1);
            pending_space = 0;
            buffer_put(&b, (char *)&src[i], 1);
        }
        i++;
    }

    if(b.len == 0) return NULL;
    return b.data;
}

Module java_parse(const char *path, const char *src, size_t len) {
    Module m = {0};
    m.filename = xstrndup(path, strlen(path));

    size_t i = 0;
    while(i < len) {
        if(src[i] == '/' && i + 1 < len && src[i + 1] == '/') {
            i = skip_line_comment(src, i, len);
            continue;
        }

        if(src[i] == '/' && i + 1 < len && src[i + 1] == '*') {
            size_t content_start = i + 2;
            size_t end = content_start;
            while(end < len && !(src[end] == '*' && end + 1 < len && src[end + 1] == '/')) end++;
            size_t content_end = end < len ? end : len;
            size_t next = end < len ? end + 2 : len;

            Symbol sym = {0};
            if(parse_doc_comment(src + content_start, content_end - content_start, &sym)) {
                const char *raw_start;
                size_t raw_len;
                trim(src + content_start, content_end - content_start, &raw_start, &raw_len);
                sym.raw_comment = xstrndup(raw_start, raw_len);
                sym.signature = extract_signature(src, next, len);
                module_push(&m, sym);
            }

            i = next;
            continue;
        }

        size_t after_ws = skip_whitespace(src, i, len);
        i = after_ws > i ? after_ws : i + 1;
    }

    return m;
}
