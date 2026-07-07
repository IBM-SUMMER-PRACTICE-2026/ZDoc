/*
 * zdoc-doc-extractor — CLI driver.
 *
 * Walks a project directory, invokes the right language parser on every
 * source file it finds, and prints the combined documentation-model JSON
 * on stdout (consumed by the renderers).
 */
#include "doc_extractor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ZDOC_DOC_EXTRACTOR_VERSION "0.1.0"

int main(int argc, char **argv) {
    const char *parser_dir = NULL;
    const char *root_dir = NULL;

    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "--version") == 0) {
            puts("zdoc-doc-extractor " ZDOC_DOC_EXTRACTOR_VERSION);
            return 0;
        }
        if(strcmp(argv[i], "--help") == 0) {
            puts("usage: zdoc-doc-extractor [--parser-dir DIR] <root_dir>\n"
                 "Walks root_dir, runs the appropriate parser on every source\n"
                 "file found, and prints the combined documentation-model\n"
                 "JSON on stdout. --parser-dir looks for parser binaries there\n"
                 "instead of relying on PATH.");
            return 0;
        }
        if(strcmp(argv[i], "--parser-dir") == 0 && i + 1 < argc) { parser_dir = argv[++i]; continue; }
        if(argv[i][0] == '-') {
            fprintf(stderr, "zdoc-doc-extractor: unknown option '%s'\n", argv[i]);
            return 2;
        }
        root_dir = argv[i];
    }

    if(!root_dir) {
        fprintf(stderr, "usage: zdoc-doc-extractor [--parser-dir DIR] <root_dir>\n");
        return 2;
    }

    DxModel model;
    if(!dx_build(root_dir, parser_dir, &model)) {
        fprintf(stderr, "zdoc-doc-extractor: %s: could not walk directory\n", root_dir);
        return 1;
    }

    dx_write(&model, stdout);
    dx_free(&model);
    return 0;
}
