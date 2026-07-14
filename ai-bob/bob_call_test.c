/* bob_call_test — standalone tool to exercise bob_call() against the real
 * bob CLI (mirrors ai/zdoc_ai.c's role for the online-mode annotate step).
 *
 * Parses one source file with the real parser to get a Module (so the
 * prompt lists real functions/lines), builds the bob prompt, calls
 * bob_call() and prints exactly what came back over the pipe.
 *
 *     make -C ai-bob bob_call_test
 *     ./ai-bob/bob_call_test path/to/File.c [--bob <bob-binary>]
 */
#include "bob-client.h"
#include "../parser/parser_interface.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Stub: this test calls bob_call() directly, not bob_client(), but
 * bob-client.c's bob_client() references append_diagrams() so the linker
 * needs a definition. append_diagrams itself isn't implemented anywhere in
 * this codebase yet (a separate, pre-existing gap). */
enum ZDoc_Error append_diagrams(const char * response, size_t response_len, Module * module) {
    (void)response; (void)response_len; (void)module;
    return ZDOC_OK;
}

int main(int argc, char **argv)
{
    const char *path = NULL;
    char *cli = "bob"; /* bob_call takes char*, not const char* */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--bob") == 0 && i + 1 < argc)
            cli = argv[++i];
        else if (argv[i][0] != '-')
            path = argv[i];
    }
    if (!path) {
        fprintf(stderr, "usage: %s <file.c|.java|.plx> [--bob <bob-binary>]\n", argv[0]);
        return 2;
    }

    enum Language lang;
    if (language_from_name(path, &lang) != ZDOC_OK) {
        fprintf(stderr, "bob_call_test: unsupported file type (need .c, .java or .plx): %s\n", path);
        return 2;
    }

    Module *mod = parse_file(lang, path);
    if (!mod) {
        fprintf(stderr, "bob_call_test: parse failed for %s\n", path);
        return 1;
    }
    printf("bob_call_test: parsed %d symbol(s) from %s\n", mod->symbolCount, path);

    char *prompt = NULL;
    enum ZDoc_Error rc = build_bob_prompt(path, mod, &prompt);
    if (rc != ZDOC_OK) {
        fprintf(stderr, "bob_call_test: build_bob_prompt failed (rc=%d)\n", rc);
        return 1;
    }

    char *response = NULL;
    size_t response_len = 0;
    rc = bob_call(prompt, &response, &response_len, cli);
    free(prompt);

    if (rc != ZDOC_OK) {
        fprintf(stderr, "bob_call_test: bob_call failed (rc=%d) - is '%s' on PATH and authenticated?\n", rc, cli);
        free(response);
        return 1;
    }

    printf("bob_call_test: got %zu byte response\n\n", response_len);
    printf("--- raw response ---\n%s\n--- end ---\n", response);

    free(response);
    return 0;
}
