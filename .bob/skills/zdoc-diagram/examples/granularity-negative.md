# Negative example — too detailed

The most common mistake is diagramming source lines instead of logic. For
the C golden example (`remove_key`), this answer is **WRONG**:

```mermaid
flowchart TD
    A[Entry: remove_key] --> B[Set cur to g_head]
    B --> C[Set prev to NULL]
    C --> D{cur not NULL?}
    D -- Yes --> E(Call strcmp)
    E --> F{strcmp returned 0?}
    F -- Yes --> G{prev is NULL?}
    G -- Yes --> H[Set g_head to cur next]
    G -- No --> I[Set prev next to cur next]
    H --> J(Call free)
    I --> J
    J --> K[Return RC_OK]
    F -- No --> L[Set prev to cur]
    L --> M[Set cur to cur next]
    M --> D
    D -- No --> N[Return RC_NOTFOUND]
```

Why it is wrong:

- **Two init assignments** (`B`, `C`) are one logical step: "Start at list
  head".
- **`strcmp` and `free` are library calls**, not documented project symbols
  from `CALLEES:` — they are mechanics inside a step, never call nodes.
- **The `prev == NULL` branch** (`G`/`H`/`I`) doesn't change the outcome,
  only the unlink mechanics — one "Unlink node from list" step.
- **Raw identifiers** (`RC_OK`, `g_head`, `cur next`) leak source text into
  labels; the declarations exist so you can write "Return RC=0" and
  "Advance to next node".

The correct answer is in [c-example-1.md](c-example-1.md): nine nodes, one
per logical step, `call` nodes only for symbols listed under `CALLEES:`.
