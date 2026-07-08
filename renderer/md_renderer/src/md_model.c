/*
 * Parses the documentation-model JSON (see md_renderer.h for the schema)
 * into an MdModel, using json.h's generic reader. Also owns freeing the
 * model and reconstructing directory/file paths from the parent-index
 * tables.
 */
#include "md_renderer.h"
#include "json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void field_param(JParser *j, const char *key, void *ctx) {
    MdParam *out = ctx;
    if(strcmp(key, "name") == 0) out->name = jparse_string(j);
    else if(strcmp(key, "desc") == 0) out->desc = jparse_string(j);
    else jskip_value(j);
}

static int parse_param(JParser *j, MdParam *out) {
    memset(out, 0, sizeof *out);
    return parse_object(j, field_param, out);
}

static void field_doc(JParser *j, const char *key, void *ctx) {
    MdSymbol *sym = ctx;
    if(strcmp(key, "brief") == 0) {
        sym->brief = jparse_string(j);
    } else if(strcmp(key, "returns") == 0) {
        sym->returns = jparse_string(j);
    } else if(strcmp(key, "notes") == 0) {
        sym->notes = jparse_string(j);
    } else if(strcmp(key, "params") == 0) {
        if(!jeat(j, '[')) return;
        size_t cap = 0;
        if(jpeek(j) != ']') {
            for(;;) {
                if(sym->param_count == cap) {
                    cap = cap ? cap * 2 : 4;
                    sym->params = xrealloc(sym->params, cap * sizeof(MdParam));
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

static int parse_doc(JParser *j, MdSymbol *sym) {
    return parse_object(j, field_doc, sym);
}

static void field_symbol(JParser *j, const char *key, void *ctx) {
    MdSymbol *out = ctx;
    if(strcmp(key, "kind") == 0) out->kind = jparse_string(j);
    else if(strcmp(key, "name") == 0) out->name = jparse_string(j);
    else if(strcmp(key, "signature") == 0) out->signature = jparse_string(j);
    else if(strcmp(key, "line") == 0) out->line = (uint32_t)jparse_int(j);
    else if(strcmp(key, "doc") == 0) parse_doc(j, out);
    else jskip_value(j);
}

static int parse_symbol(JParser *j, MdSymbol *out) {
    memset(out, 0, sizeof *out);
    return parse_object(j, field_symbol, out);
}

static void field_file(JParser *j, const char *key, void *ctx) {
    MdFile *out = ctx;
    if(strcmp(key, "name") == 0) {
        out->name = jparse_string(j);
    } else if(strcmp(key, "language") == 0) {
        out->language = jparse_string(j);
    } else if(strcmp(key, "parent_dir_index") == 0) {
        out->parent_dir_index = (int)jparse_int(j);
    } else if(strcmp(key, "symbols") == 0) {
        if(!jeat(j, '[')) return;
        size_t cap = 0;
        if(jpeek(j) != ']') {
            for(;;) {
                if(out->symbol_count == cap) {
                    cap = cap ? cap * 2 : 8;
                    out->symbols = xrealloc(out->symbols, cap * sizeof(MdSymbol));
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

static int parse_file(JParser *j, MdFile *out) {
    memset(out, 0, sizeof *out);
    out->parent_dir_index = -1;
    return parse_object(j, field_file, out);
}

static void field_dir(JParser *j, const char *key, void *ctx) {
    MdDir *out = ctx;
    if(strcmp(key, "name") == 0) out->name = jparse_string(j);
    else if(strcmp(key, "parent_index") == 0) out->parent_index = (int)jparse_int(j);
    else jskip_value(j);
}

static int parse_dir(JParser *j, MdDir *out) {
    memset(out, 0, sizeof *out);
    out->parent_index = -1;
    return parse_object(j, field_dir, out);
}

int md_model_parse(const char *json, size_t len, MdModel *out) {
    memset(out, 0, sizeof *out);
    JParser jp = { json, json + len, 1 };
    JParser *j = &jp;

    if(!jeat(j, '{')) { md_model_free(out); memset(out, 0, sizeof *out); return 0; }
    if(jpeek(j) != '}') {
        size_t dir_cap = 0, file_cap = 0;
        for(;;) {
            char *key = jparse_string(j);
            if(!j->ok) { free(key); md_model_free(out); memset(out, 0, sizeof *out); return 0; }
            jeat(j, ':');
            if(key && strcmp(key, "dirs") == 0) {
                if(!jeat(j, '[')) { free(key); goto fail; }
                if(jpeek(j) != ']') {
                    for(;;) {
                        if(out->dir_count == dir_cap) {
                            dir_cap = dir_cap ? dir_cap * 2 : 4;
                            out->dirs = xrealloc(out->dirs, dir_cap * sizeof(MdDir));
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
                            out->files = xrealloc(out->files, file_cap * sizeof(MdFile));
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

fail:
    md_model_free(out);
    memset(out, 0, sizeof *out);
    return 0;
}

static void free_symbol(MdSymbol *s) {
    free(s->kind);
    free(s->name);
    free(s->signature);
    free(s->brief);
    free(s->returns);
    free(s->notes);
    for(size_t k = 0; k < s->param_count; k++) {
        free(s->params[k].name);
        free(s->params[k].desc);
    }
    free(s->params);
}

void md_model_free(MdModel *m) {
    for(size_t i = 0; i < m->dir_count; i++) free(m->dirs[i].name);
    free(m->dirs);

    for(size_t i = 0; i < m->file_count; i++) {
        MdFile *f = &m->files[i];
        free(f->name);
        free(f->language);
        for(size_t k = 0; k < f->symbol_count; k++) free_symbol(&f->symbols[k]);
        free(f->symbols);
    }
    free(m->files);

    memset(m, 0, sizeof *m);
}

/* Reconstructs a path by walking parent_index links up to the root (-1),
 * then joining the names back together root-to-leaf. */
int md_dir_path(const MdModel *m, int dir_index, char *out, size_t out_size) {
    if(dir_index < 0) { if(out_size) out[0] = '\0'; return 0; }

    /* Collect the chain of indices from leaf to root, then print root-first. */
    int chain[256];
    int depth = 0;
    int idx = dir_index;
    while(idx >= 0 && depth < (int)(sizeof chain / sizeof *chain)) {
        chain[depth++] = idx;
        idx = m->dirs[idx].parent_index;
    }

    size_t used = 0;
    for(int k = depth - 1; k >= 0; k--) {
        const char *name = m->dirs[chain[k]].name ? m->dirs[chain[k]].name : "";
        size_t need = strlen(name) + (used > 0 ? 1 : 0);
        if(used + need >= out_size) return -1;
        if(used > 0) out[used++] = '/';
        memcpy(out + used, name, strlen(name));
        used += strlen(name);
    }
    out[used] = '\0';
    return 0;
}

int md_file_path(const MdModel *m, size_t file_index, char *out, size_t out_size) {
    const MdFile *f = &m->files[file_index];
    char dir_path[1024];
    if(md_dir_path(m, f->parent_dir_index, dir_path, sizeof dir_path) != 0) return -1;

    const char *name = f->name ? f->name : "";
    size_t need = strlen(dir_path) + (dir_path[0] ? 1 : 0) + strlen(name) + 1;
    if(need > out_size) return -1;

    if(dir_path[0]) {
        snprintf(out, out_size, "%s/%s", dir_path, name);
    } else {
        snprintf(out, out_size, "%s", name);
    }
    return 0;
}
