/*
 * zdoc-c-parser — CLI driver for the ZDoc C/C++ parser.
 *
 * Parses the given source files and prints the extracted symbols in a
 * human-readable layout on stdout (mirroring the plx/java parser demos).
 */
#include "c_parser.h"

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

/* Print a doc block's fields (brief/returns/notes/params) indented. */
static void print_doc(FILE *o, const cp_doc *d)
{
    fprintf(o, "    Brief      : %s\n", or_null(d->brief));
    fprintf(o, "    Returns    : %s\n", or_null(d->returns));
    fprintf(o, "    Notes      : %s\n", or_null(d->notes));
    fprintf(o, "    Params (%zu) :", d->nparams);
    if (d->nparams == 0) {
        fputs(" (none)\n", o);
    } else {
        fputc('\n', o);
        for (size_t i = 0; i < d->nparams; i++)
            fprintf(o, "      - %s - %s\n", or_null(d->params[i].name),
                    or_null(d->params[i].desc));
    }
}

static int emit_module(FILE *o, const char *path)
{
    cp_result *r = cp_parse_file(path);
    if (!r || cp_error(r)) {
        fprintf(stderr, "zdoc-c-parser: %s: %s\n", path,
                r ? cp_error(r) : "out of memory");
        cp_result_free(r);
        return 1;
    }

    size_t n;
    const cp_symbol *syms = cp_symbols(r, &n);

    fprintf(o, "Module: %s (language: %s)\n", path, lang_of(path));
    fprintf(o, "Documented symbols: %zu\n", n);

    cp_doc md;
    if (cp_module_doc(r, &md)) {
        fputs("\nModule doc:\n", o);
        print_doc(o, &md);
    }

    for (size_t i = 0; i < n; i++) {
        const cp_symbol *s = &syms[i];
        fprintf(o, "\n[%zu] %s\n", i + 1, or_null(s->name));
        fprintf(o, "    Name       : %s\n", or_null(s->name));
        fprintf(o, "    Signature  : %s\n", or_null(s->signature));
        fprintf(o, "    Line       : %u\n", s->line);
        fprintf(o, "    Type       : %s\n", cp_symbol_kind_name(s->kind));
        if (s->has_doc) {
            print_doc(o, &s->doc);
        } else {
            fputs("    Brief      : (null)\n", o);
            fputs("    Returns    : (null)\n", o);
            fputs("    Notes      : (null)\n", o);
            fputs("    Params (0) : (none)\n", o);
        }
    }

    cp_result_free(r);
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
             "Parses C/C++ sources and prints extracted symbols + doc\n"
             "comments as JSON on stdout (one \"modules\" entry per file).");
        return 0;
    }

    int rc = 0;
    for (int i = 1; i < argc; i++) {
        if (i > 1)
            fputc('\n', stdout);
        rc |= emit_module(stdout, argv[i]);
    }
    return rc;
}
