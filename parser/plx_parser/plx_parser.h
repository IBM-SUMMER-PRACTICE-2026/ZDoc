#ifndef PLX_PARSER_H
#define PLX_PARSER_H

#include "str_helpers.h"          /* defines the Line slice type */
#include "../shared/parser_shared.h"

Module *plx_parse_file(const char *path);

#endif
