#ifndef PLX_PARSER_H
#define PLX_PARSER_H

#include <stddef.h>

#include "str_helpers.h"
#include "../shared/parser_shared.h"

/* A single source line: for now a heap-allocated, NUL-terminated copy plus its
 * length (bytes, excluding the terminator). The copy is a temporary staging
 * step - a later migration will point `data` straight into the mapped file
 * buffer. `data` is NULL to signal end-of-input. */
typedef struct {
    char *data;
    size_t len;
} Line;

Module *plx_parse_file(const char *path);

#endif
