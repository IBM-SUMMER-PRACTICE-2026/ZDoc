/*
 * zdoc-md-renderer — CLI driver for the ZDoc Markdown renderer.
 *
 * Reads the normalised documentation-model JSON (from doc_extractor) from a
 * file argument or stdin, and writes Markdown files under --out-dir: one
 * per source module plus a root index.md linking to all of them.
 */
#include "md_renderer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ZDOC_MD_RENDERER_VERSION "0.1.0"

/* Reads all of f into a growable buffer - used for both a file argument and
 * stdin, since stdin has no length to fseek/ftell for up front. */
static char *read_all(FILE *f, size_t *out_len) {
    size_t cap = 1 << 16, len = 0;
    char *buf = malloc(cap);
    if(!buf) { fprintf(stderr, "zdoc-md-renderer: out of memory\n"); exit(1); }

    size_t n;
    while((n = fread(buf + len, 1, cap - len, f)) > 0) {
        len += n;
        if(len == cap) {
            cap *= 2;
            char *nbuf = realloc(buf, cap);
            if(!nbuf) { free(buf); fprintf(stderr, "zdoc-md-renderer: out of memory\n"); exit(1); }
            buf = nbuf;
        }
    }
    *out_len = len;
    return buf;
}

int main(int argc, char **argv) {
    const char *out_dir = "./zdoc-out";
    const char *title = NULL;
    const char *input_path = NULL;

    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "--version") == 0) {
            puts("zdoc-md-renderer " ZDOC_MD_RENDERER_VERSION);
            return 0;
        }
        if(strcmp(argv[i], "--help") == 0) {
            puts("usage: zdoc-md-renderer [--out-dir DIR] [--title TITLE] [file.json]\n"
                 "Renders the ZDoc documentation-model JSON (from doc_extractor) as\n"
                 "Markdown: one .md file per module plus a root index.md.\n"
                 "Reads the given file, or stdin if no file is given.");
            return 0;
        }
        if(strcmp(argv[i], "--out-dir") == 0 && i + 1 < argc) { out_dir = argv[++i]; continue; }
        if(strcmp(argv[i], "--title") == 0 && i + 1 < argc) { title = argv[++i]; continue; }
        if(argv[i][0] == '-') {
            fprintf(stderr, "zdoc-md-renderer: unknown option '%s'\n", argv[i]);
            return 2;
        }
        input_path = argv[i];
    }

    char *json;
    size_t len;
    if(input_path) {
        FILE *f = fopen(input_path, "rb");
        if(!f) {
            fprintf(stderr, "zdoc-md-renderer: %s: could not open file\n", input_path);
            return 1;
        }
        json = read_all(f, &len);
        fclose(f);
    } else {
        json = read_all(stdin, &len);
    }

    MdModel model;
    int parsed = md_model_parse(json, len, &model);
    free(json);
    if(!parsed) {
        fprintf(stderr, "zdoc-md-renderer: malformed JSON input\n");
        return 1;
    }

    int rc = md_render(&model, out_dir, title);
    if(rc != 0) fprintf(stderr, "zdoc-md-renderer: failed writing output to '%s'\n", out_dir);

    md_model_free(&model);
    return rc == 0 ? 0 : 1;
}
