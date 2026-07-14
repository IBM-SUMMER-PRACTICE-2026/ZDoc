/* zdoc_ai — local developer tool for AI Assisted (online) mode.
 *
 * Runs Bob over ONE source file and prints a Mermaid block diagram per symbol,
 * each tagged with the symbol's starting line — the key that ties a diagram to
 * the function it belongs to. This is the intended developer workflow: Bob is
 * local, invoked per file, not a codebase-wide daemon pass.
 *
 *     make -C ai zdoc_ai
 *     ./ai/zdoc_ai path/to/File.(c|java|plx) [--bob <bob-binary>]
 *
 * It uses the real parser (parse_file) to get each symbol and its starting
 * line, then zdoc_ai_annotate_file to slice + diagram per symbol. Requires bob
 * on PATH (or --bob) with a valid session/API key; the diagram contract ships
 * in the prompt, so no Bob extension is needed. Output is Markdown so it can be
 * piped straight into docs.
 */
#include "ai_mode.h"
#include "../parser/parser_interface.h"
#include "bob_client/bob_client.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    const char *path = NULL;
    const char *bob_cli = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--bob") == 0 && i + 1 < argc)
            bob_cli = argv[++i];
        else if (argv[i][0] != '-')
            path = argv[i];
    }

    if (!path) {
        fprintf(stderr,
                "usage: %s <file.c|.java|.plx> [--bob <bob-binary>]\n",
                argv[0]);
        return 2;
    }

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

    AiOptions opt;
    opt.bob = bob_config_default();
    if (bob_cli)
        opt.bob.cli = bob_cli;

    int n = zdoc_ai_annotate_file(path, mod, &opt);
    if (n < 0) {
        fprintf(stderr, "zdoc_ai: annotation error\n");
        return 1;
    }

    printf("# %s — %d symbol(s), %d diagrammed\n", path, mod->symbolCount, n);
    for (int i = 0; i < mod->symbolCount; i++) {
        Symbol *s = &mod->symbols[i];
        printf("\n## line %u: %s\n", s->line, s->name ? s->name : "(unnamed)");
        if (s->diagram && s->diagram[0])
            printf("```mermaid\n%s\n```\n", s->diagram);
        else
            printf("_(no diagram)_\n");
    }

    return (n > 0) ? 0 : 1;
}
