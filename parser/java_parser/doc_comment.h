// Parses the text inside a single /* ... */ doc comment into a Symbol.
#ifndef JAVA_PARSER_DOC_COMMENT_H
#define JAVA_PARSER_DOC_COMMENT_H

#include <stddef.h>
#include "java_parser.h"

//Parse the text inside a /* ... */ doc comment into a Symbol. Returns 1 and fills *out if the comment contains
//a recognized Method:/Routine:/Function:/Logic:/@param label, otherwise returns 0 and *out is untouched (nothing allocated survives).
int parse_doc_comment(const char *content, size_t clen, Symbol *out);

#endif
