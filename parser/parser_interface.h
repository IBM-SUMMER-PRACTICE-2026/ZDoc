#ifndef PARSER_INTERFACE_H
#define PARSER_INTERFACE_H

#include "../parser/plx_parser/plx_parser.h"
#include "../parser/java_parser/java_parser.h"
#include "../parser/c_parser/c_parser.h"

enum Language{
    C = 0x00,
    JAVA = 0x01,
    PLX = 0x02
};

enum Language language_from_name(const char* name);

Module* parse_file(enum Language lan, const char* path);

#endif