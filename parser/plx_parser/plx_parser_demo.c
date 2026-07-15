/*
 * plx_parser_demo — CLI demo for the PL/X doc-comment parser.
 *
 * Parses each source file given as an argument and prints the extracted
 * doc-comment symbols to stdout.
 */

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "str_helpers.h"
#include "plx_parser.h"
#include "../shared/parser_shared.h"

/**
 * @brief Check whether path has one of the recognized PL/X file extensions.
 *
 * Recognizes ".plx", ".pls", ".plas" and ".plxmac", compared
 * case-insensitively.
 *
 * @param path The file path to check.
 * @return Non-zero if path's extension is recognized, 0 otherwise
 *         (including when path has no extension at all).
 */
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

/**
 * @brief CLI entry point: parse each given PL/X file and print its symbols.
 *
 * For every argument, validates the file extension via
 * has_known_extension(), parses the file with plx_parse_file(), prints the
 * resulting Module to stdout via print_module(), and reports the parse
 * time to stderr. Continues on a per-file failure rather than aborting,
 * accumulating a non-zero exit code.
 *
 * @param argc Argument count.
 * @param argv Argument vector; argv[1..argc) are the files to parse.
 * @return 0 if every file parsed successfully, 1 if any file failed to
 *         parse, or 2 for a usage error (no files given, or an
 *         unsupported extension).
 */
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
