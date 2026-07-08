/*
 Parses the documentation-model JSON (see html_model.h for the schema)
 into an HtmlModel, using json.h's generic reader. Also owns freeing the
 model and reconstructing directory/file paths from the parent-index
 tables.
 */
#include "html_model.h"
#include "json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*parse_object callbacks: one per object type in the schema.
parse_object calls the callback once per key and the callback consumes
that key's value; unknown keys are jskip_value'd so new fields in the
JSON don't break older renderers.
 */
static void field_param(JParser *j, const char *key, void *ctx) {
    HtmlParam *out = ctx;
    if(strcmp(key, "name") == 0) out->name = jparse_string(j);
    else if(strcmp(key, "desc") == 0) out->desc = jparse_string(j);
    else jskip_value(j);
}

static int parse_param(JParser *j, HtmlParam *out) {
    memset(out, 0, sizeof *out);
    return parse_object(j, field_param, out);
}

static void field_doc(JParser *j, const char *key, void *ctx) {
    HtmlSymbol *sym = ctx;
    if(strcmp(key, "brief") == 0) {
        sym->brief = jparse_string(j);
    } else if(strcmp(key, "returns") == 0) {
        sym->returns = jparse_string(j);
    } else if(strcmp(key, "notes") == 0) {
        sym->notes = jparse_string(j);
    } else if(strcmp(key, "diagram") == 0) {
        sym->diagram = jparse_string(j);
    } else if(strcmp(key, "refs") == 0) {
        if(!jeat(j, '[')) return;
        size_t cap = 0;
        if(jpeek(j) != ']') {
            for(;;) {
                if(sym->ref_count == cap) {
                    cap = cap ? cap * 2 : 4;
                    sym->refs = xrealloc(sym->refs, cap * sizeof(char *));
                }
                sym->refs[sym->ref_count] = jparse_string(j);
                if(!j->ok) return;
                sym->ref_count++;
                if(jpeek(j) == ',') { jeat(j, ','); continue; }
                break;
            }
        }
        jeat(j, ']');
    } else if(strcmp(key, "params") == 0) {
        if(!jeat(j, '[')) return;
        //Grow-by-doubling array; cap is only needed while parsing
        size_t cap = 0;
        if(jpeek(j) != ']') {
            for(;;) {
                if(sym->param_count == cap) {
                    cap = cap ? cap * 2 : 4;
                    sym->params = xrealloc(sym->params, cap * sizeof(HtmlParam));
                }
                if(!parse_param(j, &sym->params[sym->param_count])) return;
                sym->param_count++;
                if(jpeek(j) == ',') { jeat(j, ','); continue; }
                break;
            }
        }
        jeat(j, ']');
    } else {
        jskip_value(j);
    }
}

static int parse_doc(JParser *j, HtmlSymbol *sym) {
    return parse_object(j, field_doc, sym);
}

static void field_symbol(JParser *j, const char *key, void *ctx) {
    HtmlSymbol *out = ctx;
    if(strcmp(key, "kind") == 0) out->kind = jparse_string(j);
    else if(strcmp(key, "name") == 0) out->name = jparse_string(j);
    else if(strcmp(key, "signature") == 0) out->signature = jparse_string(j);
    else if(strcmp(key, "line") == 0) out->line = (uint32_t)jparse_int(j);
    else if(strcmp(key, "doc") == 0) parse_doc(j, out);
    else jskip_value(j);
}

static int parse_symbol(JParser *j, HtmlSymbol *out) {
    memset(out, 0, sizeof *out);
    return parse_object(j, field_symbol, out);
}

static void field_file(JParser *j, const char *key, void *ctx) {
    HtmlFile *out = ctx;
    if(strcmp(key, "name") == 0) {
        out->name = jparse_string(j);
    } else if(strcmp(key, "language") == 0) {
        out->language = jparse_string(j);
    } else if(strcmp(key, "parent_dir_index") == 0) {
        out->parent_dir_index = (int)jparse_int(j);
    } else if(strcmp(key, "error") == 0) {
        //The extractor emits "error":true when a file's parser failed
        out->error = (jpeek(j) == 't');
        jskip_value(j);
    } else if(strcmp(key, "symbols") == 0) {
        if(!jeat(j, '[')) return;
        //Same doubling growth as the params array
        size_t cap = 0;
        if(jpeek(j) != ']') {
            for(;;) {
                if(out->symbol_count == cap) {
                    cap = cap ? cap * 2 : 8;
                    out->symbols = xrealloc(out->symbols, cap * sizeof(HtmlSymbol));
                }
                if(!parse_symbol(j, &out->symbols[out->symbol_count])) return;
                out->symbol_count++;
                if(jpeek(j) == ',') { jeat(j, ','); continue; }
                break;
            }
        }
        jeat(j, ']');
    } else {
        jskip_value(j);
    }
}

//parent_dir_index defaults to -1 (root level) until the JSON says otherwise
static int parse_file(JParser *j, HtmlFile *out) {
    memset(out, 0, sizeof *out);
    out->parent_dir_index = -1;
    return parse_object(j, field_file, out);
}

static void field_dir(JParser *j, const char *key, void *ctx) {
    HtmlDir *out = ctx;
    if(strcmp(key, "name") == 0) out->name = jparse_string(j);
    else if(strcmp(key, "parent_index") == 0) out->parent_index = (int)jparse_int(j);
    else jskip_value(j);
}

//parent_index defaults to -1 (a root directory)
static int parse_dir(JParser *j, HtmlDir *out) {
    memset(out, 0, sizeof *out);
    out->parent_index = -1;
    return parse_object(j, field_dir, out);
}
//Parser for the documentation-model JSON (see html_model.h for the schema).
int html_model_parse(const char *json, size_t len, HtmlModel *out) {
    memset(out, 0, sizeof *out);
    JParser jp = { json, json + len, 1 };
    JParser *j = &jp;
    //If the top-level object is malformed, free any memory allocated so far and return 0.
    if(!jeat(j, '{')) { html_model_free(out); memset(out, 0, sizeof *out); return 0; }
    if(jpeek(j) != '}') {
        size_t dir_cap = 0, file_cap = 0;
        for(;;) {
            char *key = jparse_string(j);
            if(!j->ok) { free(key); html_model_free(out); memset(out, 0, sizeof *out); return 0; }
            jeat(j, ':');
            //Only "dirs" and "files" are recognised at the top level; anything else is skipped
            if(key && strcmp(key, "dirs") == 0) {
                if(!jeat(j, '[')) { free(key); goto fail; }
                if(jpeek(j) != ']') {
                    for(;;) {
                        if(out->dir_count == dir_cap) {
                            dir_cap = dir_cap ? dir_cap * 2 : 4;
                            out->dirs = xrealloc(out->dirs, dir_cap * sizeof(HtmlDir));
                        }
                        if(!parse_dir(j, &out->dirs[out->dir_count])) { free(key); goto fail; }
                        out->dir_count++;
                        if(jpeek(j) == ',') { jeat(j, ','); continue; }
                        break;
                    }
                }
                jeat(j, ']');
            } else if(key && strcmp(key, "files") == 0) {
                if(!jeat(j, '[')) { free(key); goto fail; }
                if(jpeek(j) != ']') {
                    for(;;) {
                        if(out->file_count == file_cap) {
                            file_cap = file_cap ? file_cap * 2 : 8;
                            out->files = xrealloc(out->files, file_cap * sizeof(HtmlFile));
                        }
                        if(!parse_file(j, &out->files[out->file_count])) { free(key); goto fail; }
                        out->file_count++;
                        if(jpeek(j) == ',') { jeat(j, ','); continue; }
                        break;
                    }
                }
                jeat(j, ']');
            } else {
                jskip_value(j);
            }
            free(key);
            if(!j->ok) goto fail;
            if(jpeek(j) == ',') { jeat(j, ','); continue; }
            break;
        }
    }
    
    if(!jeat(j, '}')) goto fail;
    return 1;

//Mid-parse failure free the half-built model so the caller has nothing to clean up
fail:
    html_model_free(out);
    memset(out, 0, sizeof *out);
    return 0;
}
//Free a symbol's memory, including its doc and params. Safe to call on a zeroed symbol.
static void free_symbol(HtmlSymbol *s) {
    free(s->kind);
    free(s->name);
    free(s->signature);
    free(s->brief);
    free(s->returns);
    free(s->notes);
    free(s->diagram);
    for(size_t k = 0; k < s->ref_count; k++) free(s->refs[k]);
    free(s->refs);
    for(size_t k = 0; k < s->param_count; k++) {
        free(s->params[k].name);
        free(s->params[k].desc);
    }
    free(s->params);
}
//Free the model's memory, leaving it zeroed. Safe to call on a zeroed model.
void html_model_free(HtmlModel *m) {
    for(size_t i = 0; i < m->dir_count; i++) free(m->dirs[i].name);
    free(m->dirs);

    for(size_t i = 0; i < m->file_count; i++) {
        HtmlFile *f = &m->files[i];
        free(f->name);
        free(f->language);
        for(size_t k = 0; k < f->symbol_count; k++) free_symbol(&f->symbols[k]);
        free(f->symbols);
    }
    free(m->files);

    memset(m, 0, sizeof *m);
}
