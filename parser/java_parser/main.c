#include <stdio.h>
#include <stdlib.h>
#include "java_parser.h"

static char *read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if(!f) {
        fprintf(stderr, "zdoc: could not open '%s'\n", path);
        exit(1);
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if(size < 0) {
        fprintf(stderr, "zdoc: could not read '%s'\n", path);
        fclose(f);
        exit(1);
    }
    rewind(f);

    char *buf = malloc((size_t)size ? (size_t)size : 1);
    if(!buf) {
        fprintf(stderr, "zdoc: out of memory\n");
        fclose(f);
        exit(1);
    }

    size_t n = fread(buf, 1, (size_t)size, f);
    fclose(f);
    *out_len = n;
    return buf;
}

int main(int argc, char **argv) {
    if(argc != 2) {
        fprintf(stderr, "usage: %s <file.java>\n", argv[0]);
        return 1;
    }

    size_t len;
    char *src = read_file(argv[1], &len);
    Module m = java_parse(argv[1], src, len);
    free(src);

    for(size_t i = 0; i < m.count; i++) {
        Symbol *s = &m.symbols[i];
        printf("== %s ==\n", s->name ? s->name : "(no name)");
        printf("signature: %s\n", s->signature ? s->signature : "(none)");
        printf("brief:   %s\n", s->brief ? s->brief : "(none)");
        printf("returns: %s\n", s->returns ? s->returns : "(none)");
        printf("notes:   %s\n", s->notes ? s->notes : "(none)");
        for(size_t p = 0; p < s->param_count; p++) printf("  param %s: %s\n", s->params[p].name, s->params[p].description ? s->params[p].description : "(none)");
        printf("\n");
    }

    module_free(&m);
    return 0;
}
