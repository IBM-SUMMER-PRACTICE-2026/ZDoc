/* Vendored minimal JSON reader/writer (DOM, arena-backed). Enough for the
 * ZDoc pipeline JSON; no external dependencies. */
#ifndef ZDOC_BC_JSON_H
#define ZDOC_BC_JSON_H

#include <stddef.h>
#include <stdio.h>

#include "util.h"

typedef enum { ZJ_NULL, ZJ_BOOL, ZJ_NUM, ZJ_STR, ZJ_ARR, ZJ_OBJ } zj_type;

typedef struct zj_val zj_val;
struct zj_val {
    zj_type t;
    const char *key;  /* set when this value is an object member */
    zj_val *next;     /* next sibling */
    /* payload */
    double num;
    int boolean;
    const char *str;
    zj_val *child;    /* first child (ZJ_ARR / ZJ_OBJ) */
    zj_val *tail;     /* last child, O(1) append */
};

typedef struct {
    bc_arena A;
    zj_val *root;
    const char *err;  /* NULL on success */
    size_t err_pos;
} zj_doc;

/* Parse; always returns a doc — check ->err. Free with zj_doc_free. */
zj_doc *zj_parse(const char *s, size_t n);
void zj_doc_free(zj_doc *d);

/* Lookups (NULL-tolerant). */
zj_val *zj_get(const zj_val *obj, const char *key);
zj_val *zj_at(const zj_val *arr, size_t i);
size_t zj_len(const zj_val *v);
const char *zj_str(const zj_val *v, const char *dflt);
double zj_num(const zj_val *v, double dflt);

/* Construction (into a doc's arena). */
zj_val *zj_new(zj_doc *d, zj_type t);
zj_val *zj_new_str(zj_doc *d, const char *s);
zj_val *zj_new_bool(zj_doc *d, int b);
void zj_push(zj_val *arr, zj_val *v);
/* Insert or replace obj[key]. */
void zj_set(zj_doc *d, zj_val *obj, const char *key, zj_val *v);

/* Compact write with full escaping. */
void zj_write(FILE *f, const zj_val *v);

#endif
