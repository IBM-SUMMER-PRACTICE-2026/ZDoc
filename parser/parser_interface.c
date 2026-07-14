#include "parser_interface.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief Determine a Language from a file's extension.
 *
 * Looks at the last '.' in name and matches it against the supported
 * extensions (.c, .java, .plx).
 *
 * @param name File name or path to inspect.
 * @param lang Set to the matching Language on success; left untouched on
 *             failure.
 * @return ZDOC_OK if name has a recognized extension, or
 *         ZDOC_UNSUPPORTED_LANGUAGE if it has no extension or an
 *         unrecognized one.
 */
enum ZDoc_Error language_from_name(const char* name, enum Language* lang) {
    const char* ext = strrchr(name, '.');
    if (ext == NULL) {
        return ZDOC_UNSUPPORTED_LANGUAGE;
    }
    if (strcmp(ext, ".c") == 0)    {*lang =  C;   return ZDOC_OK;}
    if (strcmp(ext, ".cpp") == 0)  {*lang = CPLUSPLUS;  return ZDOC_OK;}
    if (strcmp(ext, ".java") == 0) {*lang = JAVA; return ZDOC_OK;}
    if (strcmp(ext, ".plx") == 0)  {*lang = PLX;  return ZDOC_OK;}
    if (strcmp(ext, ".plxmac") == 0)  {*lang = PLXMAC;  return ZDOC_OK;}
    return ZDOC_UNSUPPORTED_LANGUAGE;
}

/**
 * @brief Dispatch to the language-specific parser for path.
 *
 * @param lan Language to parse path as (C, JAVA, or PLX).
 * @param path Path to the source file to parse.
 * @return The parsed Module, or NULL if lan is not a recognized Language
 *         or the underlying parser failed.
 */
Module* parse_file(enum Language lan, const char* path) {

    Module* result = NULL;

    switch (lan)
    {
    case C:
    case CPLUSPLUS:
        result = cp_parser(path);
        break;
    case JAVA:
        result = java_parse(path);
        break;
    case PLXMAC:
    case PLX:
        result = plx_parse_file(path);
        break;

    default:
        fprintf(stderr, "parser_interface: unsupported language format\n");
    }

    return result;
}