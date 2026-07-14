/*
 * ZDoc AI Assisted mode — the single entry point that turns a parsed tree into
 * an annotated one.
 *
 * This is the interface the rest of ZDoc calls to "run online mode": given the
 * module_tree tables and the parsed Module array (exactly what the renderers
 * already take), it fills in each Symbol.diagram by invoking the Bob CLI once
 * per symbol. Downstream, both renderers already emit Symbol.diagram when it is
 * present, so wiring online mode into a pipeline is just:
 *
 *     parse(...)  ->  zdoc_ai_annotate(...)  ->  md_render/html_render(...)
 *
 * The pipeline wiring itself (CLI/daemon) is intentionally NOT done here — see
 * the "For the daemon" section of the pull request. This layer is standalone
 * and testable on its own (ai/ai_mode_live.c drives it against the real Bob).
 *
 * Part of the ZDoc ai/ layer (see docs/ZDOC.md). Portable: all OS-specific work
 * is behind the bob_client spawn seam.
 */
#ifndef ZDOC_AI_MODE_H
#define ZDOC_AI_MODE_H

#include <stddef.h>

#include "../parser/shared/parser_shared.h"      /* Module, Symbol            */
#include "../extractor/doc_extractor/module_tree/modtree_tables.h" /* tables   */
#include "bob_client/bob_client.h"               /* BobConfig                 */

/* How to run the online pass. Kept deliberately small; concurrency and
 * per-call timeouts are daemon-side concerns tracked in the PR handoff, not
 * baked into this interface yet. */
typedef struct {
    BobConfig bob; /* how to reach Bob (cli + extra args); bob_config_default() */
} AiOptions;

/* Annotate every symbol in `modules` in place: for each one, slice its body
 * from the source file, build the zdoc-diagram snippet, call Bob, and store the
 * sanitized Mermaid flowchart in Symbol.diagram (any previous value freed).
 *
 * Each module is matched to its source file through Module.pathIndex and the
 * module_tree tables — the same mapping the renderers use — so the source is
 * resolved the same way regardless of parse order.
 *
 * Failure policy is silent-skip: a symbol whose body can't be sliced or whose
 * Bob call fails is left with diagram == NULL (the renderers simply omit the
 * diagram for it). Returns the number of symbols successfully annotated, or -1
 * on an unrecoverable error (bad arguments / allocation failure).
 */
int zdoc_ai_annotate(const modtree_dir_table_t *dirs,
                     const modtree_file_table_t *files,
                     Module *modules, size_t module_count,
                     const AiOptions *opt);

#endif /* ZDOC_AI_MODE_H */
