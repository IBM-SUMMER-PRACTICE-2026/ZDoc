/*
 * zdoc-java-parser — CLI driver for the ZDoc Java parser.
 *
 * Parses the given source files and prints the extracted symbols in a
 * human-readable layout on stdout (mirroring the plx_parser demo format).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "java_parser.h"

#define ZDOC_JAVA_PARSER_VERSION "0.1.0"

static int emit_module(const char *path) {
    Module *m = java_parse(path);
    if(!m) {
        fprintf(stderr, "zdoc-java-parser: %s: out of memory\n", path);
        return 1;
    }

    print_module(m);
    free_module(m);
    
    return 0;
}

int main(int argc, char **argv) {
    static char obuf[1 << 16];
    setvbuf(stdout, obuf, _IOFBF, sizeof obuf);

    if(argc < 2) {
        fprintf(stderr,
                "usage: zdoc-java-parser <file.java> [more files]\n"
                "       zdoc-java-parser --version\n");
        return 2;
    }
    if(strcmp(argv[1], "--version") == 0) {
        puts("zdoc-java-parser " ZDOC_JAVA_PARSER_VERSION);
        return 0;
    }
    if(strcmp(argv[1], "--help") == 0) {
        puts("usage: zdoc-java-parser <source files>\n"
             "Parses Java sources and prints extracted symbols + doc\n"
             "comments as JSON on stdout (one \"modules\" entry per file).");
        return 0;
    }

    int rc = 0;
    for(int i = 1; i < argc; i++) {
        if(i > 1) fputs("\n", stdout);
        rc |= emit_module(argv[i]);
    }
    return rc;
}
