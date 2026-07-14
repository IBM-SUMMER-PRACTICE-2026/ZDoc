/* Online-mode annotation — the step that runs AFTER parsing.
 *
 * ZDoc parses each file once (daemon/CLI, upstream). Online mode is a separate
 * pass over that already-parsed model: it does NOT parse. Given a module's
 * source `path` and its already-parsed `mod` (each Symbol already carries its
 * starting line), it runs Bob once — Bob reads the file itself — and stores the
 * flowchart Bob returns for each function into the Symbol whose starting line
 * matches. The starting line is the match key.
 *
 * The standalone `zdoc_ai` tool parses a file only to obtain a Module to hand
 * here; the daemon/CLI instead pass the Module they already parsed, so nothing
 * is parsed twice.
 */
#ifndef ZDOC_AI_ANNOTATE_H
#define ZDOC_AI_ANNOTATE_H

#include "../parser/shared/parser_shared.h" /* Module, Symbol */

/* Annotate an already-parsed module in place: fill each Symbol.diagram with the
 * flowchart Bob returns for the function at that symbol's starting line.
 * `bob_cli` is the Bob binary (NULL -> "bob"). Returns the number of diagrams
 * stored, or -1 if Bob could not be run. */
int zdoc_ai_annotate(const char *path, Module *mod, const char *bob_cli);

#endif /* ZDOC_AI_ANNOTATE_H */
