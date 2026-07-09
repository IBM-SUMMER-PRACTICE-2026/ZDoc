# Graph conventions for ZDoc diagrams

The output is a raw Mermaid `flowchart TD` block, inserted verbatim by ZDoc.
Everything below exists to keep that block valid and self-sanitized.

## Node shapes

| Role | Mermaid syntax | Rendered as | Use for |
|------|----------------|-------------|---------|
| step | `id[text]` | Rectangle | Actions / processing steps |
| return | `id[text]` | Rectangle | Exit points |
| decision | `id{text}` | Diamond | Branches (phrased as a question) |
| call | `id(text)` | Rounded | Calls to symbols listed in `CALLEES:` (enables cross-reference) |

## Text labels

- Entry node: `A[Entry: NAME]` (symbol name exactly as in source).
- Returns: `Return RC=n` plus a 2–4 word reason when the code shows one
  (`Return RC=8 - storage failure`).
- Calls: `(Call TERMPROC)` — the callee's real name, exactly as listed in
  `CALLEES:`, so ZDoc can link it.
- Decisions phrased as questions: `{Storage obtained?}`
- Keep every label under ~6 words.

## Sanitization (why the raw block is safe)

ZDoc does no escaping — the text you write lands in the rendered Mermaid
unchanged. So the label charset is restricted to characters Mermaid never
misparses:

- **Allowed inside a node:** letters, digits, spaces, and `: = ? -`.
- **Never inside a node:** quotes `" '`, brackets `[]`, braces `{}`,
  parentheses `()`, pipes `|`, angle brackets `<>`, `&`, `#`, `;`, `/`,
  backticks, or any Mermaid syntax. Reword instead — `CB(len)` becomes
  `CB length`, `a && b` becomes `a and b`.

## Edges

- Plain sequential flow, no label: `A --> B`.
- Decision out-edges always labeled: `B -- Yes --> C`, `B -- No --> D`, or
  literal values (`B -- RC=8 --> D`).
- Loop back-edges (an edge to an earlier node) are allowed and encouraged
  over unrolling: `F --> C`.

## Structure

- First line is `flowchart TD`; one node/edge statement per line, 4-space
  indented.
- First node = entry; everything reachable from it.
- Ids `A`, `B`, `C`… in flow order.
- 1–14 nodes; 5–12 is the sweet spot.
- One Mermaid block per response; nothing outside the fence.
