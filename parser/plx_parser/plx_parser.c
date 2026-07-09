/*
 * plx_parser — PL/X doc-comment block parser.
 *
 * Parses the "Label: content  @TAG" doc-comment blocks that precede
 * procedures, as specified in docs/plx-doccomment-convention.md, and prints
 * the extracted symbols to stdout.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "str_helpers.h"
#include "plx_parser.h"
#include "parser_helpers.h"

/* ------------------------------------------------------------------ */
/* File parsing                                                        */
/* ------------------------------------------------------------------ */

Module *plx_parse_file(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        perror(path);
        return NULL;
    }

    Module *mod = init_module(path);
    DocBlock blk;
    char line[MAX_LINE];
    int lineNo = 0;

    int capturing = 0;
    int inProlog = 0;
    SigState sigState = { 0, 0, 0 };
    StrBuf sig;
    char *sigProc = NULL;
    int procLine = 0; /* line the PROC/ProcEntry was matched on */

    block_init(&blk);
    sb_init(&sig);

    // Iterate line by line
    while (fgets(line, sizeof line, f)) {
        char *content;
        char *procName;

        lineNo++;

        if (capturing) {
            if (sig_consume(&sig, line, &sigState)) {
                char *sigText = squeeze_ws(sb_steal(&sig));
                if (blk.active)
                    block_to_symbol(&blk, mod, sigProc, sigText, procLine);
                else
                    fprintf(stderr,
                            "%s:%d: note: procedure '%s' has no doc-comment "
                            "block - skipped\n",
                            path, procLine, sigProc);
                free(sigText);
                free(sigProc);
                sigProc = NULL;
                capturing = 0;
            }
            continue;
        }

        // Inside a Method Prolog block (.plxmac)
        if (inProlog) {
            if (is_prolog_end(line)) {
                if (blk.active)
                    blk.closed = 1; /* block done; wait for the ProcEntry */
                inProlog = 0;
            } else {
                content = prolog_content(line);
                if (*content != '\0') /* padding-only lines are skipped */
                    feed_doc_line(&blk, mod, content, lineNo);
                free(content);
            }
            continue;
        }

        if (is_prolog_start(line)) {
            if (blk.closed) {
                /* new block starts while one is pending: flush it */
                block_to_symbol(&blk, mod, NULL, NULL, lineNo);
            }
            inProlog = 1;
            blk.prolog = 1;
            continue;
        }

        content = comment_content(line);
        if (content) {
            // Padding only
            if (*content == '\0') {
                /* padding-only line: skip, keep current field */
            }
            // Banner line
            else if (is_banner(content)) {
                if (blk.active)
                    blk.closed = 1; /* block done; wait for the PROC */
            }
            // Doc comment line
            else {
                feed_doc_line(&blk, mod, content, lineNo);
            }
            free(content);
            continue;
        }

        procName = match_proc_start(line);
        if (!procName)
            procName = match_procentry(line);
        if (procName) {
            capturing = 1;
            sigState.depth = 0;
            sigState.inComment = 0;
            sigState.inString = 0;
            sigProc = procName;
            procLine = lineNo;
            if (sig_consume(&sig, line, &sigState)) {
                char *sigText = squeeze_ws(sb_steal(&sig));
                if (blk.active)
                    block_to_symbol(&blk, mod, sigProc, sigText, procLine);
                else
                    fprintf(stderr,
                            "%s:%d: note: procedure '%s' has no doc-comment "
                            "block - skipped\n",
                            path, procLine, sigProc);
                free(sigText);
                free(sigProc);
                sigProc = NULL;
                capturing = 0;
            }
        }
        /* all other code lines are irrelevant to doc extraction */
    }

    if (blk.active)
        block_to_symbol(&blk, mod, NULL, NULL, lineNo);
    if (capturing) {
        free(sigProc);
        sb_free(&sig);
    }
    module_shrink_to_fit(mod);

    fclose(f);
    return mod;
}
