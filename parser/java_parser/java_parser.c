#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "java_parser.h"
#include "util.h"
#include "doc_comment.h"

// Free the memory allocated for a symbol and its contents
static void symbol_free(Symbol *y) {
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

void module_free(Module *m) {
    for(size_t s = 0; s < m->count; s++) symbol_free(&m->symbols[s]);
    free(m->symbols);
    free(m->filename);
    m->symbols = NULL;
    m->count = m->cap = 0;
    m->filename = NULL;
}

// Append a symbol to the module's symbol array. The array's memory is expanded if necessary.
static void module_push(Module *m, Symbol sym) {
    if(m->count == m->cap) {
        m->cap = m->cap ? m->cap * 2 : 8;
        m->symbols = xrealloc(m->symbols, m->cap * sizeof(Symbol));
    }
    m->symbols[m->count++] = sym;
}

// Skip any annotations (e.g. @Override, @SuppressWarnings("unchecked"), stacked or with
// nested-paren arguments) between a doc comment and the declaration it documents.
// Returns the index of the first non-whitespace, non-annotation character.
static size_t skip_annotations(const char *src, size_t start, size_t len) {
    size_t i = start;
    for(;;) {
        size_t after_ws = skip_whitespace(src, i, len);
        if(after_ws + 1 < len && src[after_ws] == '/' && src[after_ws + 1] == '/') {
            after_ws = skip_line_comment(src, after_ws, len);
            i = after_ws; 
            continue;
        }

        if(after_ws + 1 < len && src[after_ws] == '/' && src[after_ws + 1] == '*'){
            size_t end = after_ws + 2;
            while(end < len && !(src[end] == '*' && end + 1 < len && src[end + 1] == '/')) end++;
            i = end < len ? end + 2 : len;
            continue;
        }
        if(after_ws >= len || src[after_ws] != '@') return after_ws;

        size_t j = after_ws + 1;
        while(j < len && (isalnum((unsigned char)src[j]) || src[j] == '_' || src[j] == '.')) j++;

        size_t after_name = skip_whitespace(src, j, len);
        if(after_name < len && src[after_name] =='(') {
            int depth = 0;
            j = after_name;
            do {
                if(src[j] == '(') depth++;
                else if(src[j] ==')')depth--;
                j++;
            } while(j < len && depth > 0);
        }

        i = j;
    }
}

// Take the declaration text right after a doc comment, up to (not including) the first '{' or ';',
// and collapse all whitespace runs to single spaces. Returns NULL if there was nothing there.
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
            buffer_put(&b, (char *)(src + i), 1);  // cast needed until buffer_put takes const char *
        }
        i++;
    }

    if(b.len == 0) return NULL;
    return b.data;
}

// Count the 1-based line number of a byte offset into src.
static uint32_t line_of(const char *src, size_t pos) {
    uint32_t line = 1;
    for(size_t k = 0; k < pos; k++)
        if(src[k] == '\n') line++;
    return line;
}

// Extract the method or constructor name from the signature. Returns NULL if it can't be found.
static char *extract_name(const char *sig) {
    if(!sig) return NULL;
    const char *p = strchr(sig, '(');
    if(!p) return NULL;
    while(p > sig && isspace((unsigned char)p[-1])) p--;
    const char *end = p;
    while(p > sig && (isalnum((unsigned char)p[-1]) || p[-1] == '_')) p--;
    if(p == end) return NULL;
    return xstrndup(p, (size_t)(end - p));
}

Module java_parse(const char *path, const char *src, size_t len) {
    Module m = {0};
    m.filename = xstrndup(path, strlen(path));

    size_t i = 0;
    while(i < len) {
        // Skip over string literals, character literals, and comments to find doc comments
        if(src[i] == '"' || src[i] == '\'') {
            char delim = src[i++];
            while(i < len && src[i] != delim) {
                if(src[i] == '\\') i++;  // skip escaped character
                i++;
            }
            if(i < len) i++;  // skip closing delimiter
            continue;
        }

        if(src[i] == '/' && i + 1 < len && src[i + 1] == '/') {
            i = skip_line_comment(src, i, len);
            continue;
        }

        if(src[i] == '/' && i + 1 < len && src[i + 1] == '*') {
            // Find the closing */ regardless of comment type so we always advance past it
            size_t scan_start = i + 2;
            size_t end = scan_start;
            while(end < len && !(src[end] == '*' && end + 1 < len && src[end + 1] == '/')) end++;
            size_t content_end = end < len ? end : len;
            size_t next = end < len ? end + 2 : len;

            // If it's a doc comment, parse it and extract the signature and name
            if(scan_start < len && src[scan_start] == '*') {
                size_t content_start = scan_start + 1;  // start after /**
                size_t content_len = content_end > content_start ? content_end - content_start : 0;
                Symbol sym = {0};
                if(parse_doc_comment(src + content_start, content_len, &sym)) {
                    const char *raw_start;
                    size_t raw_len;
                    trim(src + content_start, content_len, &raw_start, &raw_len);
                    sym.raw_comment = xstrndup(raw_start, raw_len);
                    size_t sig_start = skip_annotations(src, next, len);  // skip annotations so they aren't mistaken for the signature
                    sym.signature   = extract_signature(src, sig_start, len);
                    free(sym.name);
                    sym.name        = extract_name(sym.signature);  // Extract the method or constructor name from the signature
                    sym.line        = line_of(src, sig_start);
                    module_push(&m, sym);
                } else {
                    symbol_free(&sym);  //  Free any partially filled symbol if parsing failed
                }
            }

            i = next;
            continue;
        }

        size_t after_ws = skip_whitespace(src, i, len);
        i = after_ws > i ? after_ws : i + 1;
    }

    return m;
}
