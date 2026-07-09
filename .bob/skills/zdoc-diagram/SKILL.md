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
your output is a machine (the ZDoc pipeline): it takes the Mermaid block you
return and drops it, verbatim, into the function's documentation section. There
is no conversion or repair step — so the Mermaid must be **valid and
self-sanitized** exactly as written.

## Output contract (HARD — never violate)

Respond with **exactly one Mermaid `flowchart TD` block and nothing else** —
one ```mermaid fence around it, no prose before or after:

```mermaid
flowchart TD
    A[Entry: INITPROC] --> B{Storage obtained?}
    B -- No --> C[Return RC=8 - storage failure]
    B -- Yes --> D[Initialise control block]
    D --> E(Call TERMPROC)
    E --> F[Return RC=0]
```

Rules:

- First line is always `flowchart TD`. One node per line thereafter,
  4-space indented.
- Node ids are single letters `A`, `B`, `C`… in flow order. The **first node
  is the entry** and every node must be reachable from it.
- Node shape encodes its role — pick exactly one per node:
  - `[text]` — **step** (an action) and **return** (an exit point).
  - `{text}` — **decision**, phrased as a question.
  - `(text)` — **call** to another documented symbol from `CALLEES:`.
- Every out-edge of a decision must carry a label:
  `B -- Yes --> D`. Plain sequential edges carry none: `D --> E`.
- 1–14 nodes total. Node text under ~6 words.
- **Sanitize every label.** Allowed characters inside a node are letters,
  digits, spaces, and `: = ? -` only. Never put quotes, brackets `[]`,
  braces `{}`, parentheses `()`, pipes `|`, angle brackets `<>`, `&`, `#`,
  `;`, `/`, or backticks *inside* the text — reword instead of `CB(len)`
  write `CB length`. This is what keeps the raw output safe to inject.
- If the snippet cannot be diagrammed (empty body, pure data declarations),
  return the single-node graph:
  `flowchart TD` then `    A[No executable logic]`.

## What "brief" means

One node per **logical step**, not per source line.

- Target **5–12 nodes** for a typical function. If more are needed, merge
  adjacent sequential steps into one node.
- Decision nodes only for branches that change the outcome (early returns,
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
  (`NAME: <signature> — <brief>`). When the body invokes one, use a call
  node `(Call NAME)` with the exact name — ZDoc cross-links it. Library or
  intrinsic calls not listed here are mechanics inside a step, never a call
  node.
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
