#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

struct bc_ablk {
    bc_ablk *next;
    size_t used, cap;
};

void *bc_alloc(bc_arena *A, size_t n)
{
    n = (n + 15u) & ~(size_t)15u;
    bc_ablk *b = A->head;
    if (!b || b->cap - b->used < n) {
        size_t cap = n > 65536 ? n : 65536;
        b = (bc_ablk *)malloc(sizeof *b + cap);
        if (!b)
            return NULL;
        b->next = A->head;
        b->used = 0;
        b->cap = cap;
        A->head = b;
    }
    void *r = (char *)(b + 1) + b->used;
    b->used += n;
    return r;
}

char *bc_adup(bc_arena *A, const char *s, size_t n)
{
    char *d = (char *)bc_alloc(A, n + 1);
    if (!d)
        return NULL;
    memcpy(d, s, n);
    d[n] = 0;
    return d;
}

void bc_arena_free(bc_arena *A)
{
    bc_ablk *b = A->head;
    while (b) {
        bc_ablk *n = b->next;
        free(b);
        b = n;
    }
    A->head = NULL;
}

void bc_sb_add(bc_sb *s, const char *a, size_t n)
{
    if (!n)
        return;
    if (s->n + n + 1 > s->cap) {
        size_t c = s->cap ? s->cap * 2 : 128;
        while (c < s->n + n + 1)
            c *= 2;
        char *nd = (char *)realloc(s->d, c);
        if (!nd)
            return;
        s->d = nd;
        s->cap = c;
    }
    memcpy(s->d + s->n, a, n);
    s->n += n;
    s->d[s->n] = 0;
}

void bc_sb_adds(bc_sb *s, const char *z)
{
    bc_sb_add(s, z, strlen(z));
}

void bc_sb_addc(bc_sb *s, char c)
{
    bc_sb_add(s, &c, 1);
}

char *bc_sb_take(bc_sb *s)
{
    char *r = s->d;
    if (!r) {
        r = (char *)malloc(1);
        if (r)
            r[0] = 0;
    }
    s->d = NULL;
    s->n = s->cap = 0;
    return r;
}

void bc_sb_reset(bc_sb *s)
{
    free(s->d);
    s->d = NULL;
    s->n = s->cap = 0;
}

char *bc_read_fd(int fd, size_t *out_n)
{
    size_t cap = 65536, n = 0;
    char *buf = (char *)malloc(cap);
    if (!buf)
        return NULL;
    for (;;) {
        if (n + 32768 + 1 > cap) {
            cap *= 2;
            char *nb = (char *)realloc(buf, cap);
            if (!nb) {
                free(buf);
                return NULL;
            }
            buf = nb;
        }
        ssize_t r = read(fd, buf + n, 32768);
        if (r < 0) {
            free(buf);
            return NULL;
        }
        if (r == 0)
            break;
        n += (size_t)r;
    }
    buf[n] = 0;
    if (out_n)
        *out_n = n;
    return buf;
}

char *bc_read_file(const char *path, size_t *out_n)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return NULL;
    char *r = bc_read_fd(fd, out_n);
    close(fd);
    return r;
}
