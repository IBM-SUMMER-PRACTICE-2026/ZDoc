/* zdoc_ai — local developer tool for AI Assisted (online) diagrams.
 *
 * Give it ONE source file. It parses the file to find each function and its
 * starting line, then prints the Bob prompt plus that function list. Bob reads
 * the file itself and returns one Mermaid flowchart per function, each tagged
 * with its starting line so the diagram is matched back to the right symbol.
 *
 *     make -C ai zdoc_ai
 *     ./ai/zdoc_ai path/to/File.(c|java|plx)
 *
 * No Bob subprocess and no snippet assembly — this just emits the prompt and
 * the (start line, name) list; Bob opens the file. The starting line is the key
 * that ties each returned diagram to its symbol.
 */
#include "../parser/parser_interface.h"

#include <stdio.h>

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

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <file.c|.java|.plx>\n", argv[0]);
        return 2;
    }
    const char *path = argv[1];

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

    fputs(PROMPT, stdout);
    printf("\nFile to read: %s\n", path);
    printf("\nFunctions to diagram (%d), each tagged by its start line:\n",
           mod->symbolCount);
    for (int i = 0; i < mod->symbolCount; i++) {
        Symbol *s = &mod->symbols[i];
        printf("  line %u: %s\n", s->line, s->name ? s->name : "(unnamed)");
    }

    return (mod->symbolCount > 0) ? 0 : 1;
}
