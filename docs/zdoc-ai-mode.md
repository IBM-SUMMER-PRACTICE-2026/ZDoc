# ZDoc AI Assisted Mode — Design Specification

> Authoritative spec for AI Assisted mode (`--mode ai`). Supersedes the
> initial implementation brief (`zdoc-ai-mode-brief.md`); where the two
> disagree, this document wins. Offline mode is unaffected by everything
> described here.

## Overview

In AI Assisted mode ZDoc calls the IBM **Bob CLI** once per extracted
function/procedure/entry to obtain a **brief block diagram**. Compared to the
original brief, this design makes four structural upgrades:

1. **Structured graph contract** — Bob returns a small JSON graph
   (nodes + edges), not raw Mermaid. ZDoc validates the graph and serializes
   Mermaid itself, so syntax and escaping are deterministic code, not model
   behavior. A bare `flowchart TD` block is still accepted as a fallback.
2. **Content-addressed cache** — every validated diagram is cached under a
   key of `SHA-256(snippet ‖ skill-version ‖ provider-command)`. Re-runs over
   unchanged source make **zero** AI calls; interrupted runs resume for free.
3. **Parallel worker pool** — Bob calls are I/O-bound; `--ai-jobs N`
   (default 4) runs them concurrently. Output order stays deterministic.
4. **Call-edge harvesting** — `call` nodes in returned graphs are matched
   against extracted symbol names and emitted as `call_edges`, feeding the
   per-symbol *Cross-references* section and (later) the roadmap's
   cross-module call graph, at zero extra AI cost.

## Pipeline position

```
source → <lang>_parser --ai-context → doc_extractor → zdoc-bob-client → renderer
```

`zdoc-bob-client` is a standalone JSON-in → JSON-out filter (stdin/stdout, or
`--input <file>`). Offline mode simply omits it from the pipe.

## Parser contract extension (`--ai-context`)

Parsers gain an `--ai-context` flag. Without it their output is
**byte-identical** to before. With it:

- each `function` / `procedure` / `entry` symbol additionally carries:
  - `"body"` — verbatim source of the executable body
  - `"line_end"` — 1-based last line of the symbol
- each module additionally carries:

```json
"declarations": [
  { "names": ["CB", "CBEYE", "CBFLAGS", "CBNEXT", "CBPTR"],
    "line": 12,
    "text": "DCL 1 CB BASED(CBPTR),\n  2 CBEYE CHAR(4), ..." }
]
```

**Every-name rule:** a declaration is listed under *every* identifier the
parser knows it introduces (struct tag, typedef name, members, the PL/X
BASED pointer, HLASM DSECT fields / EQU labels, Pascal record fields).
Parsers may under-report exotic names: the closure builder additionally
indexes each declaration under all identifiers tokenized from its `text`,
so lookups err toward over-matching (harmless — precision comes from the
reference lookup, and the budget caps the size).

## Closure assembly

For each symbol, `zdoc-bob-client` collects only the context its body
actually references:

1. **Reference extraction** — tokenize the body with
   `[A-Za-z_$#@][A-Za-z0-9_$#@]*`, subtract a per-language keyword table.
   Over-collection is intended; unresolved names cost nothing.
2. **Declaration index** — name → declaration hash table per module.
   Case-folded lookups for PL/X, PLAS, HLASM, Pascal; case-sensitive for
   C, C++, Java. Many names map to one declaration; dedupe by pointer.
3. **Tiered closure** — tier 0 = directly referenced declarations (sorted
   for determinism); tier k+1 = declarations referenced from tier k
   (default depth 1). Character budget (default 4000); tier 0 fills before
   tier 1; if even the first declaration exceeds the budget, send it anyway.
4. **Callee context** — referenced names that resolve to extracted
   *function* symbols contribute one line each
   (`NAME: <signature> — <brief>`), not their bodies.

### Snippet format (must match the skill)

```
DOC:
<symbol's own doc comment brief, when present>

DECLARATIONS:
<decl 1 text>
<decl 2 text>

CALLEES:
TERMPROC: TERMPROC: PROC(ANCHOR) — Terminate subsystem

FUNCTION (<PL/X | PLAS | C | C++ | Java | HLASM | Pascal>):
<function body>
```

`DOC:`, `DECLARATIONS:` and `CALLEES:` sections are omitted when empty.
`FUNCTION` is always last.

## The graph contract

The skill instructs Bob to return **one JSON object** (optionally inside a
single ```json fence):

```json
{
  "nodes": [
    { "id": "A", "kind": "step",     "text": "Entry: INITPROC" },
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

Validation (mechanical, in `graph.c`):

- 1–14 nodes; unique non-empty ids; `kind` ∈ {`step`, `decision`, `call`,
  `return`}; non-empty `text`.
- Every edge references existing ids; every node reachable from the first
  node ("entry").
- Every out-edge of a `decision` node carries a `label`.

Mermaid is then serialized by ZDoc (`flowchart TD`, quoted labels, escaping
handled in code): `step` → `A["…"]`, `decision` → `B{"…"}`, `call` →
`C("…")`, `return` → `D["…"]`.

**Fallback:** if the response is not JSON but contains exactly one valid
`flowchart TD` mermaid block, ZDoc parses it into the same graph model and
proceeds. Normalization (stripping fences and surrounding prose, dropping
`%%` comments) runs before validation, so trivial wrapper noise never costs
a retry.

## Retry, fallback, logging

Per symbol — a failure never aborts the run:

1. Call Bob; normalize → parse → validate. Valid → attach.
2. Invalid/failed → retry **once**, appending a contract-violation notice to
   the snippet.
3. Still invalid → attach `diagram_error`; renderers emit
   `> *Block diagram unavailable for this function.*`
4. Every failure appends module, symbol, and raw response to
   `zdoc-ai-failures.log` (in the output directory), so bad cases can become
   new skill examples.
5. Run end: summary line to stderr —
   `generated N, cached N, repaired N, failed N`.

## Output augmentation

`zdoc-bob-client` passes the parser/extractor JSON through unchanged and adds
per symbol:

- `"block_diagram"` — validated Mermaid body (string), and/or
- `"call_edges"` — array of extracted-symbol names the diagram calls, or
- `"diagram_error": true` — generation failed after retry.

## Caching, resume, record/replay

- Cache dir: `~/.cache/zdoc/` (override `--ai-cache-dir`, disable
  `--ai-no-cache`, bypass reads with `--ai-refresh`).
- Cache entries are the validated **graph JSON** (not the rendered Mermaid),
  so serialization improvements re-render old cache hits.
- `--ai-record <dir>` writes request/response pairs; the test suite replays
  them without a Bob CLI present.

## Provider seam

The invocation is a config-driven command template:

```yaml
ai:
  command: "bob explain --diagram --brief --lang {lang}"   # default
```

The snippet is always delivered on **stdin** (argv has size limits; no shell
is ever involved — `posix_spawn` with an argv split on whitespace). `{lang}`
is substituted with the language name. `--bob-cli` / `--bob-args` remain as
sugar that rewrites the default template.

## Constraints

- C11, no new external dependencies (vendored minimal JSON reader/writer and
  SHA-256, both single-file).
- Deterministic output everywhere (sorted iteration) — snapshot tests
  depend on it.
- Offline mode behavior bit-identical with AI code compiled in.
- Macro-heavy HLASM/PL/X caveat stands: diagrams reflect unexpanded source.
  A pre-expansion hook remains a TODO.