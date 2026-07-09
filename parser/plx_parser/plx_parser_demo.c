/*
 * plx_parser_demo — CLI demo for the PL/X doc-comment parser.
 *
 * Parses each source file given as an argument and prints the extracted
 * doc-comment symbols to stdout.
 */

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "helpers.h"
#include "plx_parser.h"
#include "../shared/parser_shared.h"

static int has_known_extension(const char *path)
{
    static const char *exts[] = { ".plx", ".pls", ".plas", ".plxmac" };
    const char *dot = strrchr(path, '.');
    size_t i;

    if (!dot)
        return 0;
    for (i = 0; i < sizeof(exts) / sizeof(exts[0]); i++)
        if (str_ieq(dot, exts[i]))
            return 1;
    return 0;
}

int main(int argc, char **argv)
{
    int rc = 0;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <file.plx|file.plxmac> [more files...]\n", argv[0]);
        return 2;
    }

    for (int i = 1; i < argc; i++) {
        Module *mod;

        if (!has_known_extension(argv[i])) {
            fprintf(stderr,
                    "%s: unsupported file type (expected .plx, .pls, .plas "
                    "or .plxmac)\n",
                    argv[i]);
            rc = 2;
            continue;
        }

        clock_t t0 = clock();
        mod = plx_parse_file(argv[i]);
        clock_t t1 = clock();
        if (!mod) {
            rc = 1;
            continue;
        }
        if (i > 1 && rc == 0)
            printf("\n");
        print_module(mod);
        free_module(mod);
        fprintf(stderr, "%s: parsed in %.3f ms\n", argv[i],
                (double)(t1 - t0) * 1000.0 / CLOCKS_PER_SEC);
    }
    return rc;
}
