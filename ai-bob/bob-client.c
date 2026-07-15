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


/**
 * @brief Build the full prompt sent to Bob for one module.
 *
 * Concatenates the fixed instructions, the file path to read, and one
 * "line N: name" entry per symbol in module, into a single heap-allocated
 * buffer sized up front from PROMPT_INSTRUCTIONS_LEN and friends.
 *
 * @param path Path of the source file Bob should read.
 * @param module Module whose symbols are listed as functions to diagram.
 * @param prompt Receives the heap-allocated prompt string on success;
 *               caller must free() it.
 * @return ZDOC_OK on success, or ZDOC_OUT_OF_MEMORY if the buffer could
 *         not be allocated.
 */
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


/**
 * @brief Orchestrate one module's AI diagram pass: prompt, call, staple.
 *
 * Builds the prompt via build_bob_prompt(), sends it to bob_cli through
 * bob_call(), then hands the raw response to append_diagrams() to staple
 * each returned flowchart onto its matching symbol.
 *
 * @param path Path of the source file being diagrammed, forwarded into
 *             the prompt.
 * @param module Module whose symbols receive diagrams on success.
 * @param bob_cli Command used to invoke the Bob CLI subprocess.
 * @return ZDOC_OK on success, or the first ZDoc_Error encountered from
 *         prompt building, the Bob call, or diagram stapling.
 */
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

/**
 * @brief Run bob_cli as a subprocess, write prompt to its stdin, and read
 *        its full stdout back.
 *
 * Opens a fresh pipe pair per call via open_pipes_bob_comunication(), so
 * each invocation spawns and tears down its own one-shot Bob process (see
 * bob_write_message()/read_bob_message() for the EOF-driven protocol this
 * relies on).
 *
 * @param prompt Prompt text to send on the subprocess's stdin.
 * @param response Receives the heap-allocated response string on success;
 *                  caller must free() it.
 * @param response_len Receives the length of *response on success.
 * @param bob_cli Command used to invoke the Bob CLI subprocess.
 * @return ZDOC_OK on success, or ZDOC_OUT_OF_MEMORY if reading the
 *         response failed.
 */
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

/**
 * @brief Strip a leading/trailing markdown code fence from a diagram slice.
 *
 * Bob's response sometimes wraps the flowchart in a markdown code fence
 * (```mermaid ... ```) despite the prompt asking for no prose. Skips any
 * blank line(s) left between the "## line N:" header and the fence, then
 * trims a leading fence line (```, optionally followed by "mermaid") and
 * a trailing ``` line in place, so Mermaid.js never sees fence syntax as
 * diagram text.
 *
 * @param diagram In/out pointer to the start of the diagram slice;
 *                updated in place to the trimmed start.
 * @param diagram_len In/out length of the diagram slice; updated in place
 *                     to the trimmed length.
 */
static void strip_mermaid_fence(const char ** diagram, size_t * diagram_len) {
    const char * start = *diagram;
    const char * end = start + *diagram_len;

    /* Skip any blank line(s) Bob left between the "## line N:" header and
     * the fence before looking for the fence itself. */
    while (start < end && (*start == '\n' || *start == '\r' || *start == ' ' || *start == '\t')) {
        start++;
    }

    if (end - start >= 3 && strncmp(start, "```", 3) == 0) {
        const char * line_end = memchr(start, '\n', (size_t)(end - start));
        if (line_end == NULL) {
            line_end = end;
        }
        start = line_end < end ? line_end + 1 : end;
        while (start < end && (*start == '\n' || *start == '\r')) {
            start++;
        }
    }

    const char * trailing = end;
    while (trailing > start && (trailing[-1] == '\n' || trailing[-1] == '\r')) {
        trailing--;
    }
    if (trailing - start >= 3 && strncmp(trailing - 3, "```", 3) == 0) {
        const char * fence_start = trailing - 3;
        while (fence_start > start && fence_start[-1] != '\n') {
            fence_start--;
        }
        end = fence_start;
        while (end > start && (end[-1] == '\n' || end[-1] == '\r')) {
            end--;
        }
    } else {
        end = trailing;
    }

    *diagram = start;
    *diagram_len = (size_t)(end - start);
}

/**
 * @brief Staple one diagram onto a symbol, replacing any diagram it had.
 *
 * Strips a surrounding markdown fence via strip_mermaid_fence() before
 * copying, so symbol->diagram always holds bare Mermaid source.
 *
 * @param diagram Start of the diagram text slice (not necessarily
 *                NUL-terminated).
 * @param diagram_len Length of the diagram text slice in bytes.
 * @param symbol Symbol whose diagram field is replaced.
 * @return ZDOC_OK on success, or ZDOC_OUT_OF_MEMORY if the copy could not
 *         be allocated.
 */
enum ZDoc_Error append_diagram_to_symbol(const char * diagram, size_t diagram_len, Symbol * symbol) {
    strip_mermaid_fence(&diagram, &diagram_len);

    char * copy = xstrndup(diagram, diagram_len);
    if (copy == NULL) {
        return ZDOC_OUT_OF_MEMORY;
    }

    free(symbol->diagram);
    symbol->diagram = copy;
    return ZDOC_OK;
}

/**
 * @brief Split Bob's full response into per-symbol diagrams and staple
 *        each one to its matching symbol.
 *
 * Scans response for successive "## line N:" headers; the text between
 * one header and the next (or the end of the response) is treated as
 * that function's diagram and handed to append_diagram_to_symbol() for
 * the symbol whose line matches N. Headers with no matching symbol line
 * are skipped.
 *
 * @param response Full raw response text from bob_call().
 * @param response_len Length of response in bytes.
 * @param module Module whose symbols receive diagrams by matching line
 *               number.
 * @return ZDOC_OK on success, or the first ZDoc_Error encountered from
 *         append_diagram_to_symbol().
 */
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