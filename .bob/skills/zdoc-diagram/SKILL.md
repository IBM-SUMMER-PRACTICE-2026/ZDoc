---
name: zdoc-diagram
version: 1.0.0
description: >
  Generate brief block diagrams for individual functions, procedures, and
  entry points in PL/X, PLAS, C, C++, Java, HLASM Assembler, and Pascal
  source code. Used by the ZDoc documentation generator, which invokes the
  Bob CLI once per extracted symbol. Activate whenever asked to explain a
  code snippet with a diagram, especially with the flags --diagram --brief.
---

# ZDoc Block Diagram Skill

You generate a **brief block diagram** for a single function. The consumer of
your output is a machine (the ZDoc pipeline), so the output contract below is
absolute. You return a small **JSON graph**; ZDoc validates it and renders the
Mermaid itself.

## Output contract (HARD — never violate)

Respond with **exactly one JSON object** and nothing else (a single
```json fence around it is permitted; prose is not):

```json
{
  "nodes": [
    { "id": "A", "kind": "step",     "text": "Entry: FUNCNAME" },
    { "id": "B", "kind": "decision", "text": "Storage obtained?" },
    { "id": "C", "kind": "return",   "text": "Return RC=8 - storage failure" },
    { "id": "D", "kind": "call",     "text": "Call TERMPROC" }
  ],
  "edges": [
    { "from": "A", "to": "B" },
    { "from": "B", "to": "C", "label": "No" }
  ]
}
```

Rules:

- `kind` is one of: `step` (action), `decision` (branch, phrased as a
  question), `call` (invocation of another documented symbol), `return`
  (exit point).
- Node ids are single letters `A`, `B`, `C`… in flow order. The **first node
  is the entry** and every node must be reachable from it.
- Every out-edge of a `decision` node must carry a `label`
  (`Yes` / `No`, or literal values like `RC=8`).
- 1–14 nodes total. `text` under ~6 words; plain words only — ZDoc handles
  all escaping, so never add quotes, brackets, or Mermaid syntax to `text`.
- If the snippet cannot be diagrammed (empty body, pure data declarations),
  return the single-node graph:
  `{"nodes":[{"id":"A","kind":"step","text":"No executable logic"}],"edges":[]}`.

## What "brief" means

One node per **logical step**, not per source line.

- Target **5–12 nodes** for a typical function. If more are needed, merge
  adjacent sequential steps into one node.
- `decision` nodes only for branches that change the outcome (early returns,
  error paths, main loop conditions). Do not diagram every `if` that merely
  tweaks a local value.
- Loops: one node for the loop body summary plus a back-edge to the loop
  decision. Never unroll.
- Collapse straight-line sequences: "Initialise control block fields" — not
  three separate assignment nodes.

Full labeling conventions in [conventions.md](conventions.md).

## How to read the input

ZDoc sends up to four sections, `FUNCTION` always last:

- `DOC:` — the function's own doc comment. Trust it for intent and naming.
- `DECLARATIONS:` — only the declarations the function references. Use them
  to name things meaningfully — say "set init flag in control block", not
  "update CBFLAGS" — but **never diagram the declarations themselves**.
- `CALLEES:` — one line per documented function this snippet calls
  (`NAME: <signature> — <brief>`). When the body invokes one, use a `call`
  node with text `Call NAME` (exact name — ZDoc cross-links it).
- `FUNCTION (<language>):` — the body to diagram. Diagram only this.

## Golden examples

Study the pairs in `examples/` before answering. They define the expected
granularity and style per language family. The PL/X and HLASM examples are
the most important — follow their level of abstraction exactly.

- [examples/plx-example-1.md](examples/plx-example-1.md)
- [examples/c-example-1.md](examples/c-example-1.md)
- [examples/asm-example-1.md](examples/asm-example-1.md)
- [examples/granularity-negative.md](examples/granularity-negative.md) —
  a wrong (too detailed) answer and its correction.
