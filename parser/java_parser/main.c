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

static int emit_module(const char *path) {
    size_t len;
    const char *err;
    char *src = read_file(path, &len, &err);
    if(!src) {
        fprintf(stderr, "zdoc-java-parser: %s: %s\n", path, err);
        return 1;
    }

    Module m = java_parse(path, src, len);
    free(src);

    java_print_module(&m);

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
    for(int i = 1; i < argc; i++) {
        if(i > 1) fputs("\n", stdout);
        rc |= emit_module(argv[i]);
    }
    return rc;
}
