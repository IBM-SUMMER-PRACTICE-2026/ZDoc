#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "bob-client.h"
#include "process_interface/process_interface.h"


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


    #define DIAGRAM_HEADER_LABEL "## line "

    #define DIAGRAM_HEADER_LABEL_LEN 8


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
    rc = bob_call(prompt, &response, &response_len, bob_cli);
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

enum ZDoc_Error bob_call(const char * prompt, char ** response, size_t * response_len, char * bob_cli) {
    *response = NULL;
    *response_len = 0;

    h in_Rd, in_Wd, out_Rd, out_Wd;
    open_pipes_bob_comunication(bob_cli, &in_Rd, &in_Wd, &out_Rd, &out_Wd);

    bob_write_message(in_Wd, prompt, strlen(prompt));
    char * out = read_bob_message(out_Rd);

    if (out == NULL) {
        return ZDOC_OUT_OF_MEMORY;
    }

    *response = out;
    *response_len = strlen(out);
    return ZDOC_OK;
}

enum ZDoc_Error append_diagram_to_symbol(const char * diagram, size_t diagram_len, Symbol * symbol) {
    char * copy = xstrndup(diagram, diagram_len);
    if (copy == NULL) {
        return ZDOC_OUT_OF_MEMORY;
    }

    free(symbol->diagram);
    symbol->diagram = copy;
    return ZDOC_OK;
}

enum ZDoc_Error append_diagrams(const char * response, size_t response_len, Module * module) {
    const char * response_end = response + response_len;
    const char * cursor = response;

    while ((cursor = strstr(cursor, DIAGRAM_HEADER_LABEL)) != NULL) {
        uint32_t line = (uint32_t)strtoul(cursor + DIAGRAM_HEADER_LABEL_LEN, NULL, 10);

        const char * diagram = strchr(cursor, '\n');
        if (diagram == NULL) {
            break;
        }
        diagram++;

        const char * next_header = strstr(diagram, DIAGRAM_HEADER_LABEL);
        const char * diagram_end = next_header ? next_header : response_end;

        while (diagram_end > diagram && (diagram_end[-1] == '\n' || diagram_end[-1] == '\r')) {
            diagram_end--;
        }

        for (int i = 0; i < module->symbolCount; i++) {
            if (module->symbols[i].line == line) {
                enum ZDoc_Error rc = append_diagram_to_symbol(diagram, (size_t)(diagram_end - diagram), &module->symbols[i]);
                if (rc != ZDOC_OK) {
                    return rc;
                }
                break;
            }
        }

        cursor = next_header ? next_header : response_end;
    }

    return ZDOC_OK;
}