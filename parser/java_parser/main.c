/*
 * zdoc-java-parser — CLI driver for the ZDoc Java parser.
 *
 * Parses the given source files and prints one JSON document on stdout:
 *   { "zdoc_parser": "java", "modules": [ { file, language, symbols... } ] }
 * Downstream ZDoc stages (extractor, renderers, bob_client) consume this.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "java_parser.h"

#define ZDOC_JAVA_PARSER_VERSION "0.1.0"

// Read a whole file into memory. Returns NULL (and sets *err) on failure
// instead of exiting, so the caller can still emit a valid per-file JSON
// error entry and move on to the next file.
static char *read_file(const char *path, size_t *out_len, const char **err) {
    *err = NULL;
    FILE *f = fopen(path, "rb");
    if(!f) {
        *err = "could not open file";
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if(size < 0) {
        fclose(f);
        *err = "could not determine file size";
        return NULL;
    }
    rewind(f);

    char *buf = malloc((size_t)size ? (size_t)size : 1);
    if(!buf) {
        fclose(f);
        *err = "out of memory";
        return NULL;
    }

    size_t n = fread(buf, 1, (size_t)size, f);
    fclose(f);
    *out_len = n;
    return buf;
}

static void jstr(FILE *o, const char *s) {
    fputc('"', o);
    for(const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch(*p) {
            case '"':  fputs("\\\"", o); break;
            case '\\': fputs("\\\\", o); break;
            case '\n': fputs("\\n", o); break;
            case '\r': fputs("\\r", o); break;
            case '\t': fputs("\\t", o); break;
            default:
                if(*p < 0x20) fprintf(o, "\\u%04x", *p);
                else fputc(*p, o);
        }
    }
    fputc('"', o);
}

static void jdoc(FILE *o, const Symbol *s) {
    fputs("{", o);
    int first = 1;
    if(s->brief) {
        fputs("\"brief\":", o);
        jstr(o, s->brief);
        first = 0;
    }
    if(s->param_count) {
        if(!first) fputc(',', o);
        fputs("\"params\":[", o);
        for(size_t i = 0; i < s->param_count; i++) {
            if(i) fputc(',', o);
            fputs("{\"name\":", o);
            jstr(o, s->params[i].name ? s->params[i].name : "");
            fputs(",\"desc\":", o);
            jstr(o, s->params[i].description ? s->params[i].description : "");
            fputc('}', o);
        }
        fputc(']', o);
        first = 0;
    }
    if(s->returns) {
        if(!first) fputc(',', o);
        fputs("\"returns\":", o);
        jstr(o, s->returns);
        first = 0;
    }
    if(s->notes) {
        if(!first) fputc(',', o);
        fputs("\"notes\":", o);
        jstr(o, s->notes);
    }
    fputc('}', o);
}

static int emit_module(FILE *o, const char *path) {
    size_t len;
    const char *err;
    char *src = read_file(path, &len, &err);
    if(!src) {
        fprintf(stderr, "zdoc-java-parser: %s: %s\n", path, err);
        fputs("{\"file\":", o);
        jstr(o, path);
        fputs(",\"error\":true,\"symbols\":[]}", o);
        return 1;
    }

    Module m = java_parse(path, src, len);
    free(src);

    fputs("{\"file\":", o);
    jstr(o, path);
    fputs(",\"language\":\"java\"", o);

    fputs(",\"symbols\":[", o);
    for(size_t i = 0; i < m.count; i++) {
        const Symbol *s = &m.symbols[i];
        if(i) fputc(',', o);
        fputs("\n  {\"kind\":\"function\"", o);
        fprintf(o, ",\"line\":%u,\"name\":", s->line);
        jstr(o, s->name ? s->name : "");
        fputs(",\"signature\":", o);
        jstr(o, s->signature ? s->signature : "");
        fputs(",\"doc\":", o);
        jdoc(o, s);
        fputc('}', o);
    }
    fputs("\n]}", o);

    module_free(&m);
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
    fputs("{\"zdoc_parser\":\"java\",\"version\":\"" ZDOC_JAVA_PARSER_VERSION
          "\",\"modules\":[\n", stdout);
    for(int i = 1; i < argc; i++) {
        if(i > 1) fputs(",\n", stdout);
        rc |= emit_module(stdout, argv[i]);
    }
    fputs("\n]}\n", stdout);
    return rc;
}
