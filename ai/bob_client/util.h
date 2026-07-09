/* Shared small utilities for zdoc-bob-client: bump arena, string builder,
 * file slurping. No external dependencies. */
#ifndef ZDOC_BC_UTIL_H
#define ZDOC_BC_UTIL_H

#include <stddef.h>

typedef struct bc_ablk bc_ablk;
typedef struct {
    bc_ablk *head;
} bc_arena;

void *bc_alloc(bc_arena *A, size_t n);
char *bc_adup(bc_arena *A, const char *s, size_t n); /* NUL-terminated copy */
void bc_arena_free(bc_arena *A);

typedef struct {
    char *d;
    size_t n, cap;
} bc_sb;

void bc_sb_add(bc_sb *s, const char *a, size_t n);
void bc_sb_adds(bc_sb *s, const char *z);
void bc_sb_addc(bc_sb *s, char c);
/* Detach the buffer (always NUL-terminated, never NULL); caller frees. */
char *bc_sb_take(bc_sb *s);
void bc_sb_reset(bc_sb *s);

/* Read a whole stream/file into a malloc'd NUL-terminated buffer. */
char *bc_read_fd(int fd, size_t *out_n);
char *bc_read_file(const char *path, size_t *out_n);

#endif
