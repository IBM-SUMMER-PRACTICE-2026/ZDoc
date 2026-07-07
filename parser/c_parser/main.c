/*
 * zdoc-c-parser — CLI driver for the ZDoc C/C++ parser.
 *
 * Parses the given source files and prints one JSON document on stdout:
 *   { "zdoc_parser": "c", "modules": [ { file, language, symbols... } ] }
 * Downstream ZDoc stages (extractor, renderers, bob_client) consume this.
 */
#include "c_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ZDOC_C_PARSER_VERSION "0.2.0"

static unsigned g_opts; /* CP_OPT_* flags from the command line */

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

static void jstr(FILE *o, const char *s)
{
    fputc('"', o);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"':  fputs("\\\"", o); break;
        case '\\': fputs("\\\\", o); break;
        case '\n': fputs("\\n", o); break;
        case '\r': fputs("\\r", o); break;
        case '\t': fputs("\\t", o); break;
        default:
            if (*p < 0x20)
                fprintf(o, "\\u%04x", *p);
            else
                fputc(*p, o);
        }
    }
    fputc('"', o);
}

static void jdoc(FILE *o, const cp_doc *d)
{
    fputs("{", o);
    int first = 1;
    if (d->brief) {
        fputs("\"brief\":", o);
        jstr(o, d->brief);
        first = 0;
    }
    if (d->nparams) {
        if (!first)
            fputc(',', o);
        fputs("\"params\":[", o);
        for (size_t i = 0; i < d->nparams; i++) {
            if (i)
                fputc(',', o);
            fputs("{\"name\":", o);
            jstr(o, d->params[i].name ? d->params[i].name : "");
            fputs(",\"desc\":", o);
            jstr(o, d->params[i].desc ? d->params[i].desc : "");
            fputc('}', o);
        }
        fputc(']', o);
        first = 0;
    }
    if (d->returns) {
        if (!first)
            fputc(',', o);
        fputs("\"returns\":", o);
        jstr(o, d->returns);
        first = 0;
    }
    if (d->notes) {
        if (!first)
            fputc(',', o);
        fputs("\"notes\":", o);
        jstr(o, d->notes);
    }
    fputc('}', o);
}

static int emit_module(FILE *o, const char *path)
{
    cp_result *r = cp_parse_file_opts(path, g_opts);
    if (!r || cp_error(r)) {
        fprintf(stderr, "zdoc-c-parser: %s: %s\n", path,
                r ? cp_error(r) : "out of memory");
        cp_result_free(r);
        fputs("{\"file\":", o);
        jstr(o, path);
        fputs(",\"error\":true,\"symbols\":[]}", o);
        return 1;
    }

    fputs("{\"file\":", o);
    jstr(o, path);
    fprintf(o, ",\"language\":\"%s\"", lang_of(path));

    cp_doc md;
    if (cp_module_doc(r, &md)) {
        fputs(",\"module_doc\":", o);
        jdoc(o, &md);
    }

    fputs(",\"symbols\":[", o);
    size_t n;
    const cp_symbol *syms = cp_symbols(r, &n);
    for (size_t i = 0; i < n; i++) {
        const cp_symbol *s = &syms[i];
        if (i)
            fputc(',', o);
        fputs("\n  {\"kind\":\"", o);
        fputs(cp_symbol_kind_name(s->kind), o);
        fprintf(o, "\",\"line\":%u,\"name\":", s->line);
        jstr(o, s->name ? s->name : "");
        fputs(",\"signature\":", o);
        jstr(o, s->signature ? s->signature : "");
        if (s->has_doc) {
            fputs(",\"doc\":", o);
            jdoc(o, &s->doc);
        }
        if (s->body) { /* --ai-context */
            fprintf(o, ",\"line_end\":%u,\"body\":", s->line_end);
            jstr(o, s->body);
        }
        fputc('}', o);
    }
    fputs("\n]", o);

    size_t nd;
    const cp_declaration *decls = cp_declarations(r, &nd);
    if (nd) { /* --ai-context */
        fputs(",\"declarations\":[", o);
        for (size_t i = 0; i < nd; i++) {
            const cp_declaration *d = &decls[i];
            if (i)
                fputc(',', o);
            fputs("\n  {\"names\":[", o);
            for (size_t k = 0; k < d->nnames; k++) {
                if (k)
                    fputc(',', o);
                jstr(o, d->names[k]);
            }
            fprintf(o, "],\"line\":%u,\"text\":", d->line);
            jstr(o, d->text ? d->text : "");
            fputc('}', o);
        }
        fputs("\n]", o);
    }
    fputc('}', o);
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
        puts("usage: zdoc-c-parser [--ai-context] <source files>\n"
             "Parses C/C++ sources and prints extracted symbols + doc\n"
             "comments as JSON on stdout (one \"modules\" entry per file).\n"
             "--ai-context additionally emits function bodies and a\n"
             "declarations array for AI Assisted mode (docs/zdoc-ai-mode.md).");
        return 0;
    }
    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], "--ai-context") == 0)
            g_opts |= CP_OPT_AI_CONTEXT;

    int rc = 0, nfiles = 0;
    fputs("{\"zdoc_parser\":\"c\",\"version\":\"" ZDOC_C_PARSER_VERSION
          "\",\"modules\":[\n", stdout);
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--ai-context") == 0)
            continue;
        if (nfiles++)
            fputs(",\n", stdout);
        rc |= emit_module(stdout, argv[i]);
    }
    fputs("\n]}\n", stdout);
    return rc;
}
