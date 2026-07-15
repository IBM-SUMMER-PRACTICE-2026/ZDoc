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
#include "../shared/file_buffer.h"

/* ------------------------------------------------------------------ */
/* File parsing                                                        */
/* ------------------------------------------------------------------ */

/**
 * @brief Read the next line from an in-memory FileBuffer.
 *
 * Returns the next line as a slice pointing straight into buf->data - no
 * copy, no allocation, and not NUL-terminated - advancing *pos past the
 * line terminator. The returned len excludes the line terminator itself
 * (the newline and any preceding carriage return, so CRLF and LF input
 * read the same). The returned data must not be freed; it is owned by buf.
 *
 * @param buf The in-memory file buffer to read from.
 * @param pos Cursor into buf->data; advanced past the consumed line.
 * @return The next line as a slice into buf->data, or { NULL, 0 } once the
 *         buffer is fully consumed. A genuinely empty line is returned as
 *         data != NULL, len == 0.
 */
static Line buf_getline(const FileBuffer *buf, size_t *pos)
{
    Line line = { NULL, 0 };
    if (*pos >= buf->len)
        return line;

    size_t start = *pos, i = start;
    while (i < buf->len && buf->data[i] != '\n')
        i++;

    *pos = i < buf->len ? i + 1 : buf->len; /* advance past the '\n' */

    size_t contentEnd = i; /* just before the '\n' (or the buffer end) */
    if (contentEnd > start && buf->data[contentEnd - 1] == '\r')
        contentEnd--; /* drop the CR of a "\r\n" */

    line.data = buf->data + start;
    line.len = contentEnd - start;
    return line;
}

/**
 * @brief Parse one PL/X source file and extract its doc-comment symbols.
 *
 * Reads path into memory and scans it line by line, recognizing three
 * doc-comment sources: Method Prolog blocks (is_prolog_start() /
 * is_prolog_end()), ordinary single-line comments (comment_content()), and
 * banner lines that close a pending block. Once a doc-comment block is
 * open, lines are fed to it via feed_doc_line(). A PROC statement or
 * ProcEntry macro call (match_proc_start() / match_procentry()) starts
 * capturing the procedure's signature via sig_consume(); once the
 * signature is complete, the pending block (if any) is turned into a
 * Symbol via block_to_symbol(), or a "no doc-comment" note is printed to
 * stderr if there was none. Any doc-comment block still open at
 * end-of-file is flushed the same way, with a NULL signature.
 *
 * @param path Path to the PL/X source file to parse.
 * @return A newly allocated Module holding the extracted symbols, or NULL
 *         if path could not be read (read_file_buffer() already reported
 *         the failure to stderr).
 */
Module *plx_parse_file(const char *path)
{
    FileBuffer buf = read_file_buffer(path);
    if (!buf.data) /* read_file_buffer already reported the failure */
        return NULL;

    Module *mod = init_module(path);
    DocBlock blk;
    Line line = { NULL, 0 };
    size_t pos = 0;
    int lineNo = 0;

    int capturing = 0;
    int inProlog = 0;
    SigState sigState = { 0, 0, 0 };
    StrBuf sig;
    Line sigProc = { NULL, 0 };
    int procLine = 0; /* line the PROC/ProcEntry was matched on */

    block_init(&blk);
    sb_init(&sig);

    // Iterate line by line
    while ((line = buf_getline(&buf, &pos)).data) {
        Line content;
        Line procName;

        lineNo++;

        if (capturing) {
            if (sig_consume(&sig, line, &sigState)) {
                char *sigText = squeeze_ws(sb_steal(&sig));
                if (blk.active)
                    block_to_symbol(&blk, mod, sigProc, sigText, procLine);
                else
                    fprintf(stderr,
                            "%s:%d: note: procedure '%.*s' has no doc-comment "
                            "block - skipped\n",
                            path, procLine, (int)sigProc.len, sigProc.data);
                free(sigText);
                sigProc = (Line){ NULL, 0 };
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
                if (content.len > 0) /* padding-only lines are skipped */
                    feed_doc_line(&blk, mod, content, lineNo);
            }
            continue;
        }

        if (is_prolog_start(line)) {
            if (blk.closed) {
                /* new block starts while one is pending: flush it */
                block_to_symbol(&blk, mod, (Line){ NULL, 0 }, NULL, lineNo);
            }
            inProlog = 1;
            blk.prolog = 1;
            continue;
        }

        content = comment_content(line);
        if (content.data) {
            // Padding only
            if (content.len == 0) {
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
            continue;
        }

        procName = match_proc_start(line);
        if (!procName.data)
            procName = match_procentry(line);
        if (procName.data) {
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
                            "%s:%d: note: procedure '%.*s' has no doc-comment "
                            "block - skipped\n",
                            path, procLine, (int)sigProc.len, sigProc.data);
                free(sigText);
                sigProc = (Line){ NULL, 0 };
                capturing = 0;
            }
        }
        /* all other code lines are irrelevant to doc extraction */
    }

    if (blk.active)
        block_to_symbol(&blk, mod, (Line){ NULL, 0 }, NULL, lineNo);
    if (capturing) {
        sb_free(&sig);
    }
    module_shrink_to_fit(mod);

    free_file_buffer(&buf);
    return mod;
}
