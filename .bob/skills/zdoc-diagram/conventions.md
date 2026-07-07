# Graph conventions for ZDoc diagrams

## Node kinds

| kind | Rendered as | Use for |
|------|-------------|---------|
| `step` | Rectangle | Actions / processing steps |
| `decision` | Diamond | Branches (phrased as a question) |
| `call` | Rounded | Calls to other documented symbols (enables cross-reference) |
| `return` | Rectangle | Exit points |

## Text labels

- Entry node: `Entry: NAME` (symbol name exactly as in source).
- Returns: `Return RC=n` plus a 2–4 word reason when the code shows one
  (`Return RC=8 - storage failure`).
- Calls: `Call TERMPROC` — the callee's real name, exactly as listed in
  `CALLEES:`, so ZDoc can link it.
- Decisions phrased as questions: `Storage obtained?`
- Keep every label under ~6 words; no punctuation except `:`, `=`, `-`, `?`.
- Plain words only: no quotes, brackets, braces, HTML, or Mermaid syntax —
  ZDoc performs all escaping and rendering.

## Edges

- Decision out-edges always labeled: `Yes` / `No`, or literal values
  (`RC=8`).
- Loop back-edges (an edge to an earlier node) are allowed and encouraged
  over unrolling.
- No labels on plain sequential flow.

## Structure

- First node = entry; everything reachable from it.
- Ids `A`, `B`, `C`… in flow order.
- 1–14 nodes; 5–12 is the sweet spot.
- One JSON object per response; nothing outside it.
