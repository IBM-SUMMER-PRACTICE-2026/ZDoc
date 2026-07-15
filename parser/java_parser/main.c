/*
 * zdoc-java-parser — CLI driver for the ZDoc Java parser.
 *
 * Parses the given source files and prints the extracted symbols in a
 * human-readable layout on stdout (mirroring the plx_parser demo format).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "java_parser.h"

#define ZDOC_JAVA_PARSER_VERSION "0.1.0"

/**
 * @brief Parse one file and print its extracted symbols, timing the parse.
 *
 * Calls java_parse() on path. On success, prints the resulting Module via
 * print_module(), frees it, and reports the elapsed parse time to stderr.
 * On failure, prints an error line to stderr instead.
 *
 * @param path Path to the Java source file to parse and print.
 * @return 0 on success, 1 if java_parse() failed.
 */
static int emit_module(const char *path) {
    clock_t t0 = clock();
    Module *m = java_parse(path);
    clock_t t1 = clock();
    if(!m) {
        fprintf(stderr, "zdoc-java-parser: %s: failed to parse (see above)\n", path);
        return 1;
    }

    print_module(m);
    free_module(m);
    fflush(stdout);
    fprintf(stderr, "%s: parsed in %.3f ms\n", path,
                (double)(t1 - t0) * 1000.0 / CLOCKS_PER_SEC);
    
    return 0;
}

/**
 * @brief CLI entry point for zdoc-java-parser.
 *
 * Enables full stdout buffering, then handles --version and --help before
 * treating any other arguments as a list of Java source files: each is
 * parsed and printed via emit_module(), separated by a blank line, with
 * the process return code the bitwise OR of every file's result.
 *
 * @param argc Argument count.
 * @param argv Argument vector; argv[1..] are source file paths, or a
 *             single --version/--help flag.
 * @return 0 on success, 1 if any file failed to parse, or 2 if no
 *         arguments were given.
 */
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
