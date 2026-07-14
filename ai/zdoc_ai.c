/* zdoc_ai — local developer tool for AI Assisted (online) diagrams.
 *
 * Give it ONE source file. It parses the file to find each function and its
 * starting line, runs Bob once (Bob reads the file itself), then stores the
 * flowchart Bob returns for each function into that symbol's `diagram` field —
 * matched back to the right symbol by its starting line.
 *
 *     make -C ai zdoc_ai
 *     ./ai/zdoc_ai path/to/File.(c|java|plx) [--bob <bob-binary>]
 *
 * Bob is invoked with fork+exec (the prompt is a single argv element, so its
 * backticks/quotes never touch a shell). Needs bob on PATH (or --bob) with a
 * valid session/API key. POSIX (local dev tool).
 */
#define _POSIX_C_SOURCE 200809L

#include "../parser/parser_interface.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static const char *PROMPT =
    "Read the source file named below and produce a ZDoc block diagram for EACH\n"
    "function listed. First actually read the file — never infer a function's\n"
    "behaviour from its name. Diagram a function ONLY from the code in its body.\n"
    "\n"
    "Faithfulness — do NOT invent anything:\n"
    "- Every node must correspond to real code in that function's body: an\n"
    "  actual statement, branch, loop, or call. If it is not in the source, it\n"
    "  is not in the diagram.\n"
    "- Every decision {..} must be a branch literally present in the code. Every\n"
    "  call (..) must be a real call site, using the callee's exact name from\n"
    "  the source. Do NOT add error paths, returns, retries, validation, or any\n"
    "  step the code does not contain.\n"
    "- Use only identifiers and names that appear in the source; never rename,\n"
    "  translate, or guess a name.\n"
    "- Do not guess. If a listed function is not found in the file, or its body\n"
    "  is empty or pure data, output `flowchart TD` then one node\n"
    "  `    A[No executable logic]` for it. If the file cannot be read at all,\n"
    "  output `flowchart TD` then `    A[Source unavailable]` — never a made-up\n"
    "  diagram.\n"
    "- Before finishing each diagram, re-check every node and edge against the\n"
    "  function body and delete any that do not map to specific code you can\n"
    "  point to.\n"
    "\n"
    "Output: for every function, EXACTLY ONE Mermaid `flowchart TD` block\n"
    "preceded by a header line `## line <N>: <name>` (its start line), and\n"
    "nothing else — no prose.\n"
    "\n"
    "Rules for every flowchart:\n"
    "- First line is `flowchart TD`; then one node per line, 4-space indented.\n"
    "- Node ids A, B, C... in flow order; the first node is the entry and every\n"
    "  node is reachable from it.\n"
    "- Shapes: [text] = step or return, {text} = decision phrased as a question,\n"
    "  (text) = a call to another function.\n"
    "- Label every decision out-edge (e.g. `B -- Yes --> C`); leave plain\n"
    "  sequential edges unlabeled.\n"
    "- 1-14 nodes, one per LOGICAL step: merge straight-line sequences, and\n"
    "  render a loop as one body node plus a back-edge (never unrolled). Keep\n"
    "  label text under ~6 words.\n"
    "- Allowed characters inside a label: letters, digits, spaces and : = ? -\n"
    "  only. Never put quotes, brackets, braces, parentheses, pipes, <>, &, #,\n"
    "  ;, / or backticks inside label text; reword instead.\n";

/* Assemble the full prompt: contract + the file to read + the function list
 * (each with its start line). malloc'd; caller frees. */
static char *build_prompt(const char *path, const Module *mod)
{
    char *buf = NULL;
    size_t len = 0;
    FILE *f = open_memstream(&buf, &len);
    if (!f)
        return NULL;
    fputs(PROMPT, f);
    fprintf(f, "\nFile to read: %s\n", path);
    fprintf(f, "\nFunctions to diagram (%d), each tagged by its start line:\n",
            mod->symbolCount);
    for (int i = 0; i < mod->symbolCount; i++) {
        const Symbol *s = &mod->symbols[i];
        fprintf(f, "  line %u: %s\n", s->line, s->name ? s->name : "(unnamed)");
    }
    fclose(f);
    return buf;
}

/* Run `cli` with the prompt as a single argv element, capturing its stdout.
 * Returns a malloc'd NUL-terminated buffer (caller frees), or NULL if the
 * process could not run or exited non-zero. */
static char *run_bob(const char *cli, const char *prompt)
{
    int fds[2];
    if (pipe(fds) != 0)
        return NULL;

    pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return NULL;
    }
    if (pid == 0) {
        dup2(fds[1], STDOUT_FILENO);
        close(fds[0]);
        close(fds[1]);
        execlp(cli, cli, "-o", "text", "-y", prompt, (char *)NULL);
        _exit(127); /* bob not found */
    }

    close(fds[1]);
    char *out = NULL;
    size_t len = 0, cap = 0;
    char b[4096];
    ssize_t r;
    while ((r = read(fds[0], b, sizeof b)) > 0) {
        if (len + (size_t)r + 1 > cap) {
            cap = cap ? cap * 2 : 8192;
            while (cap < len + (size_t)r + 1)
                cap *= 2;
            char *g = realloc(out, cap);
            if (!g) {
                free(out);
                out = NULL;
                break;
            }
            out = g;
        }
        memcpy(out + len, b, (size_t)r);
        len += (size_t)r;
    }
    close(fds[0]);

    int st = 0;
    waitpid(pid, &st, 0);
    if (!out || !WIFEXITED(st) || WEXITSTATUS(st) != 0) {
        free(out);
        return NULL;
    }
    out[len] = '\0';
    return out;
}

/* Copy [start,end) into a fresh string with backticks and trailing whitespace
 * stripped, so it is safe to embed. malloc'd. */
static char *clean_flowchart(const char *start, const char *end)
{
    size_t span = (size_t)(end - start);
    char *d = malloc(span + 1);
    if (!d)
        return NULL;
    size_t w = 0;
    for (size_t i = 0; i < span; i++)
        if (start[i] != '`')
            d[w++] = start[i];
    while (w > 0 && (d[w - 1] == '\n' || d[w - 1] == '\r' || d[w - 1] == ' ' ||
                     d[w - 1] == '\t'))
        w--;
    d[w] = '\0';
    return d;
}

/* Split Bob's output into `## line <N>` blocks and store each block's flowchart
 * into the symbol whose starting line is N (that's the match). Returns how many
 * symbols were filled. */
static int store_diagrams(Module *mod, const char *out)
{
    int placed = 0;
    const char *p = out;
    while ((p = strstr(p, "## line ")) != NULL) {
        long n = strtol(p + 8, NULL, 10);
        const char *next = strstr(p + 8, "## line ");
        const char *end = next ? next : out + strlen(out);
        const char *fc = strstr(p + 8, "flowchart");
        if (fc && fc < end) {
            char *diagram = clean_flowchart(fc, end);
            if (diagram && diagram[0]) {
                for (int i = 0; i < mod->symbolCount; i++) {
                    if ((long)mod->symbols[i].line == n) {
                        free(mod->symbols[i].diagram);
                        mod->symbols[i].diagram = diagram;
                        diagram = NULL;
                        placed++;
                        break;
                    }
                }
            }
            free(diagram);
        }
        if (!next)
            break;
        p = next;
    }
    return placed;
}

int main(int argc, char **argv)
{
    const char *path = NULL;
    const char *cli = "bob";
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

    char *prompt = build_prompt(path, mod);
    if (!prompt) {
        fprintf(stderr, "zdoc_ai: out of memory\n");
        return 1;
    }

    char *out = run_bob(cli, prompt);
    free(prompt);
    if (!out) {
        fprintf(stderr,
                "zdoc_ai: bob call failed (is '%s' on PATH and authenticated?)\n",
                cli);
        return 1;
    }

    int placed = store_diagrams(mod, out);
    free(out);

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
