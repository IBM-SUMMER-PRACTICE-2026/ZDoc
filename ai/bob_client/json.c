#include "json.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *s, *p, *e;
    zj_doc *d;
    int depth;
} jp;

static void jerr(jp *P, const char *msg)
{
    if (!P->d->err) {
        P->d->err = msg;
        P->d->err_pos = (size_t)(P->p - P->s);
    }
}

static void jws(jp *P)
{
    while (P->p < P->e && (*P->p == ' ' || *P->p == '\t' || *P->p == '\n' ||
                           *P->p == '\r'))
        P->p++;
}

static zj_val *jval(jp *P);

static const char *jstring(jp *P)
{
    /* at opening quote */
    P->p++;
    bc_sb sb = {0};
    while (P->p < P->e) {
        unsigned char c = (unsigned char)*P->p;
        if (c == '"') {
            P->p++;
            char *tmp = bc_sb_take(&sb);
            const char *r = bc_adup(&P->d->A, tmp, strlen(tmp));
            free(tmp);
            return r;
        }
        if (c == '\\') {
            P->p++;
            if (P->p >= P->e)
                break;
            char ec = *P->p++;
            switch (ec) {
            case '"': bc_sb_addc(&sb, '"'); break;
            case '\\': bc_sb_addc(&sb, '\\'); break;
            case '/': bc_sb_addc(&sb, '/'); break;
            case 'b': bc_sb_addc(&sb, '\b'); break;
            case 'f': bc_sb_addc(&sb, '\f'); break;
            case 'n': bc_sb_addc(&sb, '\n'); break;
            case 'r': bc_sb_addc(&sb, '\r'); break;
            case 't': bc_sb_addc(&sb, '\t'); break;
            case 'u': {
                if (P->e - P->p < 4) {
                    jerr(P, "bad \\u escape");
                    bc_sb_reset(&sb);
                    return NULL;
                }
                unsigned v = 0;
                for (int i = 0; i < 4; i++) {
                    char h = P->p[i];
                    v <<= 4;
                    if (h >= '0' && h <= '9') v |= (unsigned)(h - '0');
                    else if (h >= 'a' && h <= 'f') v |= (unsigned)(h - 'a' + 10);
                    else if (h >= 'A' && h <= 'F') v |= (unsigned)(h - 'A' + 10);
                    else { jerr(P, "bad \\u escape"); bc_sb_reset(&sb); return NULL; }
                }
                P->p += 4;
                /* surrogate pair */
                if (v >= 0xD800 && v <= 0xDBFF && P->e - P->p >= 6 &&
                    P->p[0] == '\\' && P->p[1] == 'u') {
                    unsigned lo = 0;
                    int ok = 1;
                    for (int i = 0; i < 4; i++) {
                        char h = P->p[2 + i];
                        lo <<= 4;
                        if (h >= '0' && h <= '9') lo |= (unsigned)(h - '0');
                        else if (h >= 'a' && h <= 'f') lo |= (unsigned)(h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') lo |= (unsigned)(h - 'A' + 10);
                        else { ok = 0; break; }
                    }
                    if (ok && lo >= 0xDC00 && lo <= 0xDFFF) {
                        v = 0x10000 + ((v - 0xD800) << 10) + (lo - 0xDC00);
                        P->p += 6;
                    }
                }
                /* UTF-8 encode */
                if (v < 0x80) bc_sb_addc(&sb, (char)v);
                else if (v < 0x800) {
                    bc_sb_addc(&sb, (char)(0xC0 | (v >> 6)));
                    bc_sb_addc(&sb, (char)(0x80 | (v & 0x3F)));
                } else if (v < 0x10000) {
                    bc_sb_addc(&sb, (char)(0xE0 | (v >> 12)));
                    bc_sb_addc(&sb, (char)(0x80 | ((v >> 6) & 0x3F)));
                    bc_sb_addc(&sb, (char)(0x80 | (v & 0x3F)));
                } else {
                    bc_sb_addc(&sb, (char)(0xF0 | (v >> 18)));
                    bc_sb_addc(&sb, (char)(0x80 | ((v >> 12) & 0x3F)));
                    bc_sb_addc(&sb, (char)(0x80 | ((v >> 6) & 0x3F)));
                    bc_sb_addc(&sb, (char)(0x80 | (v & 0x3F)));
                }
                break;
            }
            default:
                jerr(P, "bad escape");
                bc_sb_reset(&sb);
                return NULL;
            }
            continue;
        }
        if (c < 0x20) {
            jerr(P, "control char in string");
            bc_sb_reset(&sb);
            return NULL;
        }
        bc_sb_addc(&sb, (char)c);
        P->p++;
    }
    jerr(P, "unterminated string");
    bc_sb_reset(&sb);
    return NULL;
}

static zj_val *jnewv(jp *P, zj_type t)
{
    zj_val *v = (zj_val *)bc_alloc(&P->d->A, sizeof *v);
    if (v) {
        memset(v, 0, sizeof *v);
        v->t = t;
    }
    return v;
}

static zj_val *jval(jp *P)
{
    if (++P->depth > 128) {
        jerr(P, "nesting too deep");
        return NULL;
    }
    jws(P);
    if (P->p >= P->e) {
        jerr(P, "unexpected end");
        P->depth--;
        return NULL;
    }
    char c = *P->p;
    zj_val *v = NULL;
    if (c == '{') {
        P->p++;
        v = jnewv(P, ZJ_OBJ);
        jws(P);
        if (P->p < P->e && *P->p == '}') {
            P->p++;
        } else {
            for (;;) {
                jws(P);
                if (P->p >= P->e || *P->p != '"') {
                    jerr(P, "expected object key");
                    break;
                }
                const char *k = jstring(P);
                if (!k)
                    break;
                jws(P);
                if (P->p >= P->e || *P->p != ':') {
                    jerr(P, "expected ':'");
                    break;
                }
                P->p++;
                zj_val *m = jval(P);
                if (!m)
                    break;
                m->key = k;
                if (v->tail)
                    v->tail->next = m;
                else
                    v->child = m;
                v->tail = m;
                jws(P);
                if (P->p < P->e && *P->p == ',') {
                    P->p++;
                    continue;
                }
                if (P->p < P->e && *P->p == '}') {
                    P->p++;
                    break;
                }
                jerr(P, "expected ',' or '}'");
                break;
            }
        }
    } else if (c == '[') {
        P->p++;
        v = jnewv(P, ZJ_ARR);
        jws(P);
        if (P->p < P->e && *P->p == ']') {
            P->p++;
        } else {
            for (;;) {
                zj_val *m = jval(P);
                if (!m)
                    break;
                if (v->tail)
                    v->tail->next = m;
                else
                    v->child = m;
                v->tail = m;
                jws(P);
                if (P->p < P->e && *P->p == ',') {
                    P->p++;
                    continue;
                }
                if (P->p < P->e && *P->p == ']') {
                    P->p++;
                    break;
                }
                jerr(P, "expected ',' or ']'");
                break;
            }
        }
    } else if (c == '"') {
        const char *s = jstring(P);
        if (s) {
            v = jnewv(P, ZJ_STR);
            v->str = s;
        }
    } else if (c == 't' && P->e - P->p >= 4 && !memcmp(P->p, "true", 4)) {
        P->p += 4;
        v = jnewv(P, ZJ_BOOL);
        v->boolean = 1;
    } else if (c == 'f' && P->e - P->p >= 5 && !memcmp(P->p, "false", 5)) {
        P->p += 5;
        v = jnewv(P, ZJ_BOOL);
    } else if (c == 'n' && P->e - P->p >= 4 && !memcmp(P->p, "null", 4)) {
        P->p += 4;
        v = jnewv(P, ZJ_NULL);
    } else if (c == '-' || (c >= '0' && c <= '9')) {
        char *endp = NULL;
        double d = strtod(P->p, &endp);
        if (endp == P->p) {
            jerr(P, "bad number");
        } else {
            P->p = endp;
            v = jnewv(P, ZJ_NUM);
            v->num = d;
        }
    } else {
        jerr(P, "unexpected character");
    }
    P->depth--;
    return P->d->err ? NULL : v;
}

zj_doc *zj_parse(const char *s, size_t n)
{
    zj_doc *d = (zj_doc *)calloc(1, sizeof *d);
    if (!d)
        return NULL;
    jp P = {s, s, s + n, d, 0};
    d->root = jval(&P);
    if (!d->err) {
        jws(&P);
        if (P.p != P.e)
            jerr(&P, "trailing garbage");
    }
    return d;
}

void zj_doc_free(zj_doc *d)
{
    if (!d)
        return;
    bc_arena_free(&d->A);
    free(d);
}

zj_val *zj_get(const zj_val *obj, const char *key)
{
    if (!obj || obj->t != ZJ_OBJ)
        return NULL;
    for (zj_val *m = obj->child; m; m = m->next)
        if (m->key && strcmp(m->key, key) == 0)
            return m;
    return NULL;
}

zj_val *zj_at(const zj_val *arr, size_t i)
{
    if (!arr || arr->t != ZJ_ARR)
        return NULL;
    zj_val *m = arr->child;
    while (m && i--)
        m = m->next;
    return m;
}

size_t zj_len(const zj_val *v)
{
    if (!v || (v->t != ZJ_ARR && v->t != ZJ_OBJ))
        return 0;
    size_t n = 0;
    for (zj_val *m = v->child; m; m = m->next)
        n++;
    return n;
}

const char *zj_str(const zj_val *v, const char *dflt)
{
    return (v && v->t == ZJ_STR) ? v->str : dflt;
}

double zj_num(const zj_val *v, double dflt)
{
    return (v && v->t == ZJ_NUM) ? v->num : dflt;
}

zj_val *zj_new(zj_doc *d, zj_type t)
{
    zj_val *v = (zj_val *)bc_alloc(&d->A, sizeof *v);
    if (v) {
        memset(v, 0, sizeof *v);
        v->t = t;
    }
    return v;
}

zj_val *zj_new_str(zj_doc *d, const char *s)
{
    zj_val *v = zj_new(d, ZJ_STR);
    if (v)
        v->str = bc_adup(&d->A, s, strlen(s));
    return v;
}

zj_val *zj_new_bool(zj_doc *d, int b)
{
    zj_val *v = zj_new(d, ZJ_BOOL);
    if (v)
        v->boolean = b;
    return v;
}

void zj_push(zj_val *arr, zj_val *v)
{
    if (!arr || arr->t != ZJ_ARR || !v)
        return;
    v->next = NULL;
    if (arr->tail)
        arr->tail->next = v;
    else
        arr->child = v;
    arr->tail = v;
}

void zj_set(zj_doc *d, zj_val *obj, const char *key, zj_val *v)
{
    if (!obj || obj->t != ZJ_OBJ || !v)
        return;
    zj_val *old = zj_get(obj, key);
    if (old) {
        /* replace payload in place, keep position */
        const char *k = old->key;
        zj_val *nx = old->next;
        *old = *v;
        old->key = k;
        old->next = nx;
        if (obj->tail == v)
            obj->tail = old;
        return;
    }
    v->key = bc_adup(&d->A, key, strlen(key));
    v->next = NULL;
    if (obj->tail)
        obj->tail->next = v;
    else
        obj->child = v;
    obj->tail = v;
}

static void wstr(FILE *f, const char *s)
{
    fputc('"', f);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"': fputs("\\\"", f); break;
        case '\\': fputs("\\\\", f); break;
        case '\n': fputs("\\n", f); break;
        case '\r': fputs("\\r", f); break;
        case '\t': fputs("\\t", f); break;
        default:
            if (*p < 0x20)
                fprintf(f, "\\u%04x", *p);
            else
                fputc(*p, f);
        }
    }
    fputc('"', f);
}

void zj_write(FILE *f, const zj_val *v)
{
    if (!v) {
        fputs("null", f);
        return;
    }
    switch (v->t) {
    case ZJ_NULL: fputs("null", f); break;
    case ZJ_BOOL: fputs(v->boolean ? "true" : "false", f); break;
    case ZJ_NUM:
        if (v->num == (double)(long long)v->num)
            fprintf(f, "%lld", (long long)v->num);
        else
            fprintf(f, "%.17g", v->num);
        break;
    case ZJ_STR: wstr(f, v->str ? v->str : ""); break;
    case ZJ_ARR:
        fputc('[', f);
        for (zj_val *m = v->child; m; m = m->next) {
            zj_write(f, m);
            if (m->next)
                fputc(',', f);
        }
        fputc(']', f);
        break;
    case ZJ_OBJ:
        fputc('{', f);
        for (zj_val *m = v->child; m; m = m->next) {
            wstr(f, m->key ? m->key : "");
            fputc(':', f);
            zj_write(f, m);
            if (m->next)
                fputc(',', f);
        }
        fputc('}', f);
        break;
    }
}
