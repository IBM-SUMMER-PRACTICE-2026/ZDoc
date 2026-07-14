#include <stdio.h>
#include <string.h>

#include "bob-client.h"


// Prompt components for bob with their lengths (lengths must always be manually updated)
#define PROMPT_INSTRUCTIONS_LEN 2217

#define PROMPT_INSTRUCTIONS "Read the source file named below and produce a ZDoc block diagram for EACH\n" \
    "function listed. First actually read the file — never infer a function's\n" \
    "behaviour from its name. Diagram a function ONLY from the code in its body.\n" \
    "\n" \
    "Faithfulness — do NOT invent anything:\n" \
    "- Every node must correspond to real code in that function's body: an\n" \
    "  actual statement, branch, loop, or call. If it is not in the source, it\n" \
    "  is not in the diagram.\n" \
    "- Every decision {..} must be a branch literally present in the code. Every\n" \
    "  call (..) must be a real call site, using the callee's exact name from\n" \
    "  the source. Do NOT add error paths, returns, retries, validation, or any\n" \
    "  step the code does not contain.\n" \
    "- Use only identifiers and names that appear in the source; never rename,\n" \
    "  translate, or guess a name.\n" \
    "- Do not guess. If a listed function is not found in the file, or its body\n" \
    "  is empty or pure data, output `flowchart TD` then one node\n" \
    "  `    A[No executable logic]` for it. If the file cannot be read at all,\n" \
    "  output `flowchart TD` then `    A[Source unavailable]` — never a made-up\n" \
    "  diagram.\n" \
    "- Before finishing each diagram, re-check every node and edge against the\n" \
    "  function body and delete any that do not map to specific code you can\n" \
    "  point to.\n" \
    "\n" \
    "Output: for every function, EXACTLY ONE Mermaid `flowchart TD` block\n" \
    "preceded by a header line `## line <N>: <name>` (its start line), and\n" \
    "nothing else — no prose.\n" \
    "\n" \
    "Rules for every flowchart:\n" \
    "- First line is `flowchart TD`; then one node per line, 4-space indented.\n" \
    "- Node ids A, B, C... in flow order; the first node is the entry and every\n" \
    "  node is reachable from it.\n" \
    "- Shapes: [text] = step or return, {text} = decision phrased as a question,\n" \
    "  (text) = a call to another function.\n" \
    "- Label every decision out-edge (e.g. `B -- Yes --> C`); leave plain\n" \
    "  sequential edges unlabeled.\n" \
    "- 1-14 nodes, one per LOGICAL step: merge straight-line sequences, and\n" \
    "  render a loop as one body node plus a back-edge (never unrolled). Keep\n" \
    "  label text under ~6 words.\n" \
    "- Allowed characters inside a label: letters, digits, spaces and : = ? -\n" \
    "  only. Never put quotes, brackets, braces, parentheses, pipes, <>, &, #,\n" \
    "  ;, / or backticks inside label text; reword instead.\n"


    #define FILE_TO_READ_LABEL_LEN 15

    #define FILE_TO_READ_LABEL "\nFile to read: "


    #define FUNCTIONS_LABEL_LEN 55

    #define FUNCTIONS_LABEL "\n\nFunctions to diagram, each tagged by its start line:\n"


enum ZDoc_Error build_bob_prompt(const char * path, Module * module, char ** prompt) {
    size_t cap = PROMPT_INSTRUCTIONS_LEN +
                 FILE_TO_READ_LABEL_LEN +
                 strlen(path) +
                 FUNCTIONS_LABEL_LEN +
                 ((10 + 6 + 256) * module->symbolCount) + // 6 digit number for line, 256 cap for symbol name, 10 for per-line label overhead
                 1;

    char * buf = xmalloc(sizeof(char) * cap);
    if (buf == NULL) {
        return ZDOC_OUT_OF_MEMORY;
    }

    int off = snprintf(buf, cap, "%s", PROMPT_INSTRUCTIONS);
    off += snprintf(buf + off, cap - (size_t)off, "%s%s", FILE_TO_READ_LABEL, path);
    off += snprintf(buf + off, cap - (size_t)off, "%s", FUNCTIONS_LABEL);

    for (int i = 0; i < module->symbolCount; i++) {
        Symbol * s = &module->symbols[i];
        off += snprintf(buf + off, cap - (size_t)off, "  line %u: %s\n",
                         s->line, s->name ? s->name : "(unnamed)");
    }

    *prompt = buf;
    return ZDOC_OK;
}


enum ZDoc_Error bob_client(const char * path, Module * module, char * bob_cli) {
    // Build the prompt
    char * prompt = NULL;
    enum ZDoc_Error rc = build_bob_prompt(path, module, &prompt);
    if(rc != ZDOC_OK) {
        free(prompt);
        return rc;
    }


    // Call bob
    char * response = NULL;
    size_t response_len = 0;
    rc = bob_call(prompt, response, &response_len, bob_cli);
    if(rc != ZDOC_OK) {
        free(response);
        return rc;
    }


    // Append the diagrams to the symbols
    rc = append_diagrams(response, response_len, module);
    if(rc != ZDOC_OK) {
        return rc;
    }

    
    return ZDOC_OK;
}