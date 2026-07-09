#include "parser_interface.h"
#include <stdio.h>
#include <string.h>

enum Language language_from_name(const char* name) {
    const char* ext = strrchr(name, '.');
    if (ext == NULL) {
        return (enum Language)-1;
    }
    if (strcmp(ext, ".c") == 0)    return C;
    if (strcmp(ext, ".java") == 0) return JAVA;
    if (strcmp(ext, ".plx") == 0)  return PLX;
    return (enum Language)-1;
}

Module* parse_file(enum Language lan, const char* path) {

    Module* result = NULL;

    switch (lan)
    {
    case C:
        result = cp_parse_file(path);
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