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

static const char *lang_of(const char *path)
{
    const char *dot = strrchr(path, '.');
    if (!dot)
        return "c";
    static const char *const cpp_exts[] = {".cpp", ".cxx", ".cc", ".hpp",
                                           ".hh", ".hxx", ".ipp"};
    for (size_t i = 0; i < sizeof cpp_exts / sizeof *cpp_exts; i++)
        if (strcmp(dot, cpp_exts[i]) == 0)
            return "cpp";
    return "c";
}

/* Render a possibly-NULL string as "(null)" so every field prints. */
static const char *or_null(const char *s)
{
    return s ? s : "(null)";
}

static void print_module(const Module *m, const char *path)
{
    printf("Module: %s (language: %s)\n", or_null(m->filename), lang_of(path));
    printf("Documented symbols: %d\n", m->symbolCount);

    for (int i = 0; i < m->symbolCount; i++) {
        const Symbol *s = &m->symbols[i];
        printf("\n[%d] %s\n", i + 1, or_null(s->name));
        printf("    Name       : %s\n", or_null(s->name));
        printf("    Signature  : %s\n", or_null(s->signature));
        printf("    Line       : %u\n", s->line);
        printf("    Type       : %s\n", or_null(s->type));
        printf("    Brief      : %s\n", or_null(s->description));
        printf("    Returns    : %s\n", or_null(s->output));
        printf("    Notes      : %s\n", or_null(s->notes));
        printf("    Diagram    : %s\n", or_null(s->diagram));
        printf("    Params (%d) :", s->inputCount);
        if (s->inputCount == 0) {
            fputs(" (none)\n", stdout);
        } else {
            fputc('\n', stdout);
            for (int j = 0; j < s->inputCount; j++)
                printf("      - %s - %s\n", or_null(s->input[j].name),
                       or_null(s->input[j].description));
        }
    }
}

static int emit_module(const char *path)
{
    Module *m = cp_parse_file(path);
    if (!m) {
        fprintf(stderr, "zdoc-c-parser: %s: out of memory\n", path);
        return 1;
    }
    print_module(m, path);
    cp_free_module(m);
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
