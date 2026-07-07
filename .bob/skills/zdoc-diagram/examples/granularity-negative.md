# Negative example — too detailed

The most common mistake is diagramming source lines instead of logic. For
the C golden example (`remove_key`), this answer is **WRONG**:

```json
{
  "nodes": [
    { "id": "A", "kind": "step", "text": "Entry: remove_key" },
    { "id": "B", "kind": "step", "text": "Set cur to g_head" },
    { "id": "C", "kind": "step", "text": "Set prev to NULL" },
    { "id": "D", "kind": "decision", "text": "cur not NULL?" },
    { "id": "E", "kind": "call", "text": "Call strcmp" },
    { "id": "F", "kind": "decision", "text": "strcmp returned 0?" },
    { "id": "G", "kind": "decision", "text": "prev is NULL?" },
    { "id": "H", "kind": "step", "text": "Set g_head to cur next" },
    { "id": "I", "kind": "step", "text": "Set prev next to cur next" },
    { "id": "J", "kind": "call", "text": "Call free" },
    { "id": "K", "kind": "return", "text": "Return RC_OK" },
    { "id": "L", "kind": "step", "text": "Set prev to cur" },
    { "id": "M", "kind": "step", "text": "Set cur to cur next" },
    { "id": "N", "kind": "return", "text": "Return RC_NOTFOUND" }
  ],
  "edges": []
}
```

Why it is wrong:

- **Two init assignments** (`B`, `C`) are one logical step: "Start at list
  head".
- **`strcmp` and `free` are library calls**, not documented project symbols
  from `CALLEES:` — they are mechanics inside a step, never `call` nodes.
- **The `prev == NULL` branch** (`G`/`H`/`I`) doesn't change the outcome,
  only the unlink mechanics — one "Unlink node from list" step.
- **Raw identifiers** (`RC_OK`, `g_head`, `cur next`) leak source text into
  labels; the declarations exist so you can write "Return RC=0" and
  "Advance to next node".

The correct answer is in [c-example-1.md](c-example-1.md): nine nodes, one
per logical step, `call` nodes only for symbols listed under `CALLEES:`.
