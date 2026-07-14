#include "parser_interface.h"
#include <stdio.h>
#include <string.h>

enum ZDoc_Error language_from_name(const char* name, enum Language* lang) {
    const char* ext = strrchr(name, '.');
    if (ext == NULL) {
        return ZDOC_UNSUPPORTED_LANGUAGE;
    }
    if (strcmp(ext, ".c") == 0)    {*lang =  C;   return ZDOC_OK;}
    if (strcmp(ext, ".java") == 0) {*lang = JAVA; return ZDOC_OK;}
    if (strcmp(ext, ".plx") == 0)  {*lang = PLX;  return ZDOC_OK;}
    return ZDOC_UNSUPPORTED_LANGUAGE;
}

Module* parse_file(enum Language lan, const char* path) {

    Module* result = NULL;

    switch (lan)
    {
    case C:
        result = cp_parser(path);
        break;
    case JAVA:
        result = java_parse(path);
        break;
    case PLX:
        result = plx_parse_file(path);
        break;

    default:
        fprintf(stderr, "parser_interface: unsupported language format\n");
    }

    return result;
}