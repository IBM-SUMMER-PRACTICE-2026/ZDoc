#ifndef PLX_PARSER_H
#define PLX_PARSER_H

#include "str_helpers.h"
#include "../shared/parser_shared.h"

#define MAX_LINE 1024

Module *plx_parse_file(const char *path);

#endif
