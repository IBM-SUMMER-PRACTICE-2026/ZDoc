/* zdoc_ai — standalone local developer tool for AI Assisted (online) diagrams.
 *
 * A thin front end over the online-mode annotation step (annotate.h). Online
 * mode itself does NOT parse — it annotates an already-parsed module. This tool
 * exists to run it on its own, so it parses ONE file just to obtain that module,
 * then hands it to zdoc_ai_annotate and prints the result. In the real pipeline
 * the daemon/CLI parses once and calls zdoc_ai_annotate directly (see the PR's
 * "Connecting online mode to the CLI/daemon" note).
 *
 *     make -C ai zdoc_ai
 *     ./ai/zdoc_ai path/to/File.(c|java|plx) [--bob <bob-binary>]
 */
#include "annotate.h"
#include "../parser/parser_interface.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    const char *path = NULL;
    const char *cli = NULL; /* NULL -> annotate uses "bob" */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--bob") == 0 && i + 1 < argc)
            cli = argv[++i];
        else if (argv[i][0] != '-')
            path = argv[i];
    }
    if (!path) {
        fprintf(stderr, "usage: %s <file.c|.java|.plx> [--bob <bob-binary>]\n",
                argv[0]);
        return 2;
    }

    /* Standalone only: parse the one file to obtain a module to annotate. */
    enum Language lang = language_from_name(path);
    if ((int)lang < 0) {
        fprintf(stderr,
                "zdoc_ai: unsupported file type (need .c, .java or .plx): %s\n",
                path);
        return 2;
    }
    Module *mod = parse_file(lang, path);
    if (!mod) {
        fprintf(stderr, "zdoc_ai: parse failed for %s\n", path);
        return 1;
    }

    /* The actual online-mode step: run Bob, fill each symbol->diagram by line. */
    int placed = zdoc_ai_annotate(path, mod, cli);
    if (placed < 0) {
        fprintf(stderr,
                "zdoc_ai: bob call failed (is '%s' on PATH and authenticated?)\n",
                cli ? cli : "bob");
        return 1;
    }

    /* Show that each diagram landed in its symbol's ->diagram field. */
    for (int i = 0; i < mod->symbolCount; i++) {
        Symbol *s = &mod->symbols[i];
        printf("\n## line %u: %s\n", s->line, s->name ? s->name : "(unnamed)");
        printf("%s\n", (s->diagram && s->diagram[0]) ? s->diagram
                                                     : "(no diagram)");
    }
    fprintf(stderr, "zdoc_ai: stored %d of %d diagrams\n", placed,
            mod->symbolCount);

    return placed > 0 ? 0 : 1;
}
