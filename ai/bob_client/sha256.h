/* Vendored minimal SHA-256 (FIPS 180-4) for cache keys. */
#ifndef ZDOC_BC_SHA256_H
#define ZDOC_BC_SHA256_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t h[8];
    uint64_t len;
    uint8_t buf[64];
    size_t buf_n;
} bc_sha256;

void bc_sha256_init(bc_sha256 *c);
void bc_sha256_update(bc_sha256 *c, const void *data, size_t n);
void bc_sha256_final(bc_sha256 *c, uint8_t out[32]);
/* hex[65] gets the lowercase hex digest, NUL-terminated */
void bc_sha256_hex(const void *data, size_t n, char hex[65]);

#endif
