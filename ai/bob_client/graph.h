/* Diagram graph model: parse the model's JSON (or fallback mermaid block),
 * validate mechanically, and serialize Mermaid deterministically.
 * See docs/zdoc-ai-mode.md §The graph contract. */
#ifndef ZDOC_BC_GRAPH_H
#define ZDOC_BC_GRAPH_H

#include <stddef.h>

#include "util.h"

typedef struct {
    const char *id;
    const char *kind;  /* step | decision | call | return */
    const char *text;
} bg_node;

typedef struct {
    const char *from, *to;
    const char *label; /* NULL when unlabeled */
} bg_edge;

typedef struct {
    bc_arena A;
    bg_node *nodes;
    size_t nn;
    bg_edge *edges;
    size_t ne;
} bg_graph;

/* Full pipeline: normalize (strip fences/prose) -> parse JSON, else parse a
 * mermaid flowchart TD block -> validate. NULL + err message on failure.
 * *repaired is set when normalization had to strip noise. */
bg_graph *bg_parse(const char *raw, size_t n, char *err, size_t errsz,
                   int *repaired);

/* Deterministic Mermaid serialization ("flowchart TD\n..."); malloc'd. */
char *bg_to_mermaid(const bg_graph *g);

/* Canonical compact graph JSON (for the cache); malloc'd. */
char *bg_canonical_json(const bg_graph *g);

/* Texts of `call` nodes (borrowed pointers into the graph); malloc'd array. */
const char **bg_calls(const bg_graph *g, size_t *out_n);

void bg_graph_free(bg_graph *g);

#endif
