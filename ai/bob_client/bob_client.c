/*
 * ZDoc bob_client — AI Assisted mode closure (implementation).
 *
 * Spawns the Bob CLI once per snippet and sanitizes its response into an
 * embeddable Mermaid flowchart. The snippet is passed as a single argv
 * element (never through a shell), so arbitrary source code — quotes,
 * newlines, whatever — needs no escaping and cannot inject a command.
 *
 * This is a library unit: the AI-mode CLI and the daemon call into it.
 * Part of the ZDoc ai/ layer (see docs/ZDOC.md). All OS-specific work lives
 * behind the bc_spawn_capture seam (spawn.h); this file is portable.
 */
#include "bob_client.h"
#include "spawn.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* strtok_r is POSIX; MSVC spells the same 3-arg function strtok_s. */
#if defined(_WIN32)
#define strtok_r strtok_s
#endif

BobConfig bob_config_default(void)
{
    BobConfig cfg = { NULL, NULL };
    return cfg;
}

/* Split `args` on whitespace into a NULL-terminated token array. The tokens
 * point into a single malloc'd copy of `args`, returned via *storage for the
 * caller to free once the tokens are no longer needed. *n receives the token
 * count. Returns NULL only on allocation failure. */
static char **split_args(const char *args, char **storage, size_t *n)
{
    *storage = NULL;
    *n = 0;

    if (!args || !*args)
        return calloc(1, sizeof(char *));

    char *copy = strdup(args);
    if (!copy)
        return NULL;

    size_t cap = 8, count = 0;
    char **tok = malloc(cap * sizeof(char *));
    if (!tok) {
        free(copy);
        return NULL;
    }

    char *save = NULL;
    for (char *t = strtok_r(copy, " \t\r\n", &save); t;
         t = strtok_r(NULL, " \t\r\n", &save)) {
        if (count + 1 >= cap) {
            cap *= 2;
            char **grown = realloc(tok, cap * sizeof(char *));
            if (!grown) {
                free(tok);
                free(copy);
                return NULL;
            }
            tok = grown;
        }
        tok[count++] = t;
    }
    tok[count] = NULL;

    *storage = copy;
    *n = count;
    return tok;
}

/* Build the one-shot prompt fed to Bob: the full ZDoc diagram contract plus the
 * snippet the closure assembled. The contract is carried IN THE PROMPT (not
 * only in the zdoc-diagram extension) so online mode is correct on any machine
 * with no extension linked — the extension then only adds optional per-language
 * examples. Returns a malloc'd string; caller frees. */
static char *build_prompt(const char *language, const char *snippet)
{
    static const char *pre =
        "Generate a ZDoc block diagram. Output EXACTLY ONE Mermaid "
        "`flowchart TD` block and nothing else: no prose, and do not use "
        "tools, read files, or search.\n\n"
        "Rules:\n"
        "- First line is `flowchart TD`; then one node per line, 4-space "
        "indented.\n"
        "- Node ids A, B, C... in flow order; the first node is the entry and "
        "every node is reachable from it.\n"
        "- Shapes: [text] = step or return, {text} = decision phrased as a "
        "question, (text) = a call to a name listed under CALLEES.\n"
        "- Label every decision out-edge (e.g. `B -- Yes --> C`); leave plain "
        "sequential edges unlabeled.\n"
        "- 1-14 nodes, one per LOGICAL step: merge straight-line sequences, "
        "and render a loop as one body node plus a back-edge (never "
        "unrolled). Keep label text under ~6 words.\n"
        "- Allowed characters inside a label: letters, digits, spaces and "
        ": = ? - only. Never put quotes, brackets, braces, parentheses, "
        "pipes, <>, &, #, ;, / or backticks inside label text; reword "
        "instead.\n"
        "- If the body is empty or pure data, output `flowchart TD` then a "
        "single node `    A[No executable logic]`.\n\n"
        "The request below has DOC / DECLARATIONS / CALLEES / FUNCTION "
        "sections; diagram ONLY the FUNCTION body, using the others just for "
        "naming.\n\n"
        "Language: ";
    static const char *mid = "\n\n";
    const char *lang = (language && *language) ? language : "";
    size_t lp = strlen(pre), ll = strlen(lang), lm = strlen(mid),
           ls = strlen(snippet);
    char *p = malloc(lp + ll + lm + ls + 1);
    if (!p)
        return NULL;
    char *w = p;
    memcpy(w, pre, lp); w += lp;
    memcpy(w, lang, ll); w += ll;
    memcpy(w, mid, lm); w += lm;
    memcpy(w, snippet, ls); w += ls;
    *w = '\0';
    return p;
}

/* Invoke the Bob CLI, capturing its stdout. Returns a malloc'd,
 * NUL-terminated buffer of the output (caller frees) and writes bob's exit
 * code to *exit_code. Returns NULL on spawn or read failure. The actual
 * process spawn is delegated to the portable bc_spawn_capture seam. */
static char *run_bob(const BobConfig *cfg, const char *language,
                     const char *snippet, int *exit_code)
{
    const char *cli = (cfg && cfg->cli && *cfg->cli) ? cfg->cli : "bob";

    char *prompt = build_prompt(language, snippet);
    if (!prompt)
        return NULL;

    char *args_storage = NULL;
    size_t nextra = 0;
    char **extra = split_args(cfg ? cfg->args : NULL, &args_storage, &nextra);
    if (!extra) {
        free(prompt);
        return NULL;
    }

    /* Real Bob is a one-shot prompt agent (Bob Shell). The zdoc-diagram
     * extension carries the output contract; we pass the snippet as the
     * prompt. `-o text` prints the final answer; `--chat-mode ask -y` keeps
     * it read-only and non-interactive.
     * cli -o text --chat-mode ask -y PROMPT = 7 slots. */
    char **argv = malloc((7 + nextra + 1) * sizeof(char *));
    if (!argv) {
        free(prompt);
        free(extra);
        free(args_storage);
        return NULL;
    }

    size_t k = 0;
    argv[k++] = (char *)cli;
    argv[k++] = (char *)"-o";
    argv[k++] = (char *)"text";
    argv[k++] = (char *)"--chat-mode";
    argv[k++] = (char *)"ask";
    argv[k++] = (char *)"-y";
    argv[k++] = prompt;
    for (size_t i = 0; i < nextra; i++)
        argv[k++] = extra[i];
    argv[k] = NULL;

    /* Hand the assembled argv to the portable spawn seam. argv[0] is the
     * program to exec; the snippet rides as a single argv element, so no
     * platform ever has to quote it through a shell. */
    char *out = NULL;
    int rc = bc_spawn_capture(argv, &out, exit_code);

    free(argv);
    free(prompt);
    free(extra);
    free(args_storage);

    return (rc == 0) ? out : NULL;
}

/* Extract a sanitized Mermaid flowchart from bob's raw output. Prefers a
 * fenced ```mermaid block; otherwise falls back to the first "flowchart"
 * keyword through end of text. Drops code fences, backticks, and stray
 * control characters so the result is safe to embed verbatim. Returns a
 * malloc'd string beginning "flowchart", or NULL if none is present. */
static char *extract_diagram(const char *raw)
{
    if (!raw)
        return NULL;

    const char *start, *end;
    const char *fence = strstr(raw, "```mermaid");
    if (fence) {
        start = fence + strlen("```mermaid");
        const char *close = strstr(start, "```");
        end = close ? close : raw + strlen(raw);
    } else {
        start = raw;
        end = raw + strlen(raw);
    }

    /* Anchor to the flowchart keyword within the chosen region. */
    const char *fc = strstr(start, "flowchart");
    if (!fc || fc >= end)
        return NULL;
    start = fc;

    size_t span = (size_t)(end - start);
    char *out = malloc(span + 1);
    if (!out)
        return NULL;

    size_t w = 0;
    for (size_t i = 0; i < span; i++) {
        unsigned char c = (unsigned char)start[i];
        if (c == '`')
            continue; /* never let a fence delimiter leak through */
        if (c < 0x20 && c != '\n' && c != '\t')
            continue; /* strip stray control characters */
        out[w++] = (char)c;
    }
    while (w > 0 && (out[w - 1] == '\n' || out[w - 1] == '\r' ||
                     out[w - 1] == '\t' || out[w - 1] == ' '))
        w--;
    out[w] = '\0';

    if (w == 0) {
        free(out);
        return NULL;
    }
    return out;
}

char *bob_diagram(const BobConfig *cfg, const char *language,
                  const char *snippet)
{
    if (!language || !snippet)
        return NULL;

    int code = -1;
    char *raw = run_bob(cfg, language, snippet, &code);
    if (!raw)
        return NULL;

    char *diagram = (code == 0) ? extract_diagram(raw) : NULL;
    free(raw);
    return diagram;
}

int bob_annotate(const BobConfig *cfg, const char *language,
                 const char *snippet, Symbol *sym)
{
    if (!sym)
        return -1;

    char *diagram = bob_diagram(cfg, language, snippet);
    if (!diagram)
        return -1;

    free(sym->diagram);
    sym->diagram = diagram;
    return 0;
}
