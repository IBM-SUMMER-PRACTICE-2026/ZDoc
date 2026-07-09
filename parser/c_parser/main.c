/*
 * zdoc-c-parser — CLI driver for the ZDoc C/C++ parser.
 *
 * Parses the given source files through the public cp_parse_file() entry
 * point and prints the resulting shared Module in a human-readable layout on
 * stdout (mirroring the plx/java parser demos).
 */
#include "c_parser.h"
#include "../shared/parser_shared.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ZDOC_C_PARSER_VERSION "0.1.0"

static int emit_module(const char *path)
{
    Module *m = cp_parse_file(path);
    if (!m) {
        fprintf(stderr, "zdoc-c-parser: %s: out of memory\n", path);
        return 1;
    }
    print_module(m);
    free_module(m);
    return 0;
}

int main(int argc, char **argv)
{
    static char obuf[1 << 16];
    setvbuf(stdout, obuf, _IOFBF, sizeof obuf);

    if (argc < 2) {
        fprintf(stderr,
                "usage: zdoc-c-parser <file.c|file.cpp|...> [more files]\n"
                "       zdoc-c-parser --version\n");
        return 2;
    }
    if (strcmp(argv[1], "--version") == 0) {
        puts("zdoc-c-parser " ZDOC_C_PARSER_VERSION);
        return 0;
    }
    if (strcmp(argv[1], "--help") == 0) {
        puts("usage: zdoc-c-parser <source files>\n"
             "Parses C/C++ sources and prints the extracted symbols in a\n"
             "human-readable layout on stdout (one module per file).");
        return 0;
    }

    int rc = 0;
    for (int i = 1; i < argc; i++) {
        if (i > 1)
            fputc('\n', stdout);
        rc |= emit_module(argv[i]);
    }
    return rc;
}
