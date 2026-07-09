/*
  java_parser.h is the data that represents a parsed Java language file
  and the two functions to produce and free one. Implemented int java_parser.c.

  The output structs (Symbol/Module/InputParam) are the shared ones from
  parser_shared.h. Java's fields map onto them as: brief -> description,
  returns -> output, params -> input, param_count -> inputCount. The `type`
  field has no Java logic yet and is left NULL.
*/

#ifndef JAVA_PARSER_H
#define JAVA_PARSER_H

#include <stddef.h>
#include <stdint.h>

#include "../shared/parser_shared.h"


Module *java_parse(const char *path);

// Print a module's symbols in a human-readable layout (see plx_parser demo).
void java_print_module(const Module *m);

// Free a module and its contents
void module_free(Module *m);

#endif