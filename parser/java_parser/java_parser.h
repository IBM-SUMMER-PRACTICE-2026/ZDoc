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

/*
  Read the text of the Java file and returns a module listing the methods or constructors in it.
  The file is read and the text is passed as 'src' and the lenght as 'len' without the disk being touched.
  
  The return module makes copies of everything so you can free 'src' as it returns.
  
  The memory can be released by calling module_free()

*/
Module java_parse(const char *path, const char *src, size_t len);

// Print a module's symbols in a human-readable layout (see plx_parser demo).
void java_print_module(const Module *m);

// Free a module and its contents
void module_free(Module *m);

#endif