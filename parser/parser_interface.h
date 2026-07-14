#ifndef PARSER_INTERFACE_H
#define PARSER_INTERFACE_H

#include "../parser/plx_parser/plx_parser.h"
#include "../parser/java_parser/java_parser.h"
#include "../parser/c_parser/c_parser.h"
#include "../zdoc/error_interface.h"

enum Language{
    C = 0x00,
    CPLUSPLUS = 0x01,
    JAVA = 0x02,
    PLX = 0x03,
    PLXMAC = 0x04
};

enum ZDoc_Error language_from_name(const char* name, enum Language* lang);

Module* parse_file(enum Language lan, const char* path);

#endif