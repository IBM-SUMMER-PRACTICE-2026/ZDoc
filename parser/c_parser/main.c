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
#include <time.h>

#define ZDOC_C_PARSER_VERSION "0.1.0"

/**
 * @brief Parse one source file and print its module to stdout.
 *
 * Times the call to cp_parse_file(), prints the resulting Module via
 * print_module() (or an out-of-memory diagnostic to stderr if parsing
 * failed), frees the module, and reports the elapsed parse time to
 * stderr.
 *
 * @param path path of the source file to parse and print
 * @return 0 on success, 1 if the module could not be parsed
 */
static int emit_module(const char *path)
{
    clock_t t0 = clock();
    Module *m = cp_parse_file(path);
    clock_t t1 = clock();
    if (!m) {
        fprintf(stderr, "zdoc-c-parser: %s: out of memory\n", path);
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
 * @brief CLI entry point for the C/C++ parser driver.
 *
 * Sets up a large stdout buffer, then handles `--version` and `--help`,
 * or otherwise treats each argument as a source file to parse and print
 * via emit_module(), separating successive files' output with a blank
 * line.
 *
 * @param argc argument count
 * @param argv argument vector; argv[1..] are source file paths, or
 *             `--version`/`--help`
 * @return 0 on success; 2 if no arguments were given; otherwise the
 *         bitwise OR of each emit_module() call's result
 */
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
