/*
 * ZDoc bob_client — AI Assisted mode closure.
 *
 * Turns a single function snippet into an embeddable Mermaid block diagram by
 * invoking the Bob CLI. The full diagram contract is carried in the prompt
 * (see build_prompt in bob_client.c); this layer only drives the invocation and
 * guards the output boundary so the raw model text is safe to embed.
 *
 * Part of the ZDoc ai/ layer. Invoked only in AI Assisted mode
 * (`--mode ai`); see docs/ZDOC.md.
 */
#ifndef ZDOC_BOB_CLIENT_H
#define ZDOC_BOB_CLIENT_H

#include "../../parser/shared/parser_shared.h" /* Symbol */

/* How to reach the Bob CLI. All fields optional; bob_config_default() gives
 * the conventional values. */
typedef struct {
    const char *cli;  /* bob binary name or path; NULL -> "bob" on PATH   */
    const char *args; /* extra args forwarded to bob (space-split); or NULL */
} BobConfig;

/* Conventional configuration: bob on PATH, no extra args. */
BobConfig bob_config_default(void);

/* Generate a diagram for `snippet` (the function body) written in `language`
 * ("C", "PL/X", "HLASM", "Java", ...). Returns a heap-allocated, sanitized
 * Mermaid flowchart — no ``` fence, begins with "flowchart" — which the
 * caller frees. Returns NULL on any failure (bob missing, non-zero exit,
 * empty output, or no flowchart in the response). */
char *bob_diagram(const BobConfig *cfg, const char *language,
                  const char *snippet);

/* Generate a diagram and attach it to sym->diagram, freeing any previous
 * value. Returns 0 on success, or -1 on failure with sym left unchanged. */
int bob_annotate(const BobConfig *cfg, const char *language,
                 const char *snippet, Symbol *sym);

#endif /* ZDOC_BOB_CLIENT_H */
