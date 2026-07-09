# bob_client — Bob CLI invocation & response sanitizing

Drives AI Assisted mode. Given a single function snippet it calls the Bob CLI to
obtain a brief (detailed-design level) block diagram of the body, then sanitizes
Bob's response into an embeddable Mermaid `flowchart` block for the renderers.

- **Layer:** `ai/`
- **Kind:** library unit (`bob_client.c` / `bob_client.h`) — no binary of its own.
  The AI-mode CLI and the daemon link against it and supply the snippets.
- **Status:** Implemented.

## Bob invocation

The snippet is passed as a single `execvp` argument — never through a shell — so
source code with quotes or newlines needs no escaping and cannot inject a command:

```
bob explain --diagram --brief --lang <language> --snippet <function_source>
```

- Bob's behaviour is defined by the [`zdoc-diagram` skill](../../.bob/skills/zdoc-diagram/):
  it returns one Mermaid `flowchart` block per symbol.
- Diagrams are **brief** — one box per logical step, not per source line.
- The response is sanitized before use: the `flowchart` block is extracted from
  any surrounding prose/fence, and code fences, backticks, and stray control
  characters are stripped so the raw model text is safe to embed verbatim.

## API

```c
BobConfig cfg = bob_config_default();          /* bob on PATH, no extra args */

char *d = bob_diagram(&cfg, "C", snippet);     /* -> malloc'd flowchart or NULL */
/* ... embed d ... */  free(d);

bob_annotate(&cfg, "C", snippet, sym);         /* attaches to sym->diagram */
```

`bob_diagram` returns a fence-less Mermaid flowchart (begins with `flowchart`)
that the caller frees, or NULL on any failure (bob missing, non-zero exit, empty
output, no flowchart). `bob_annotate` stores that string into `Symbol.diagram`.

## Requirements

- Bob CLI installed and on `PATH` (override with `--bob-cli <path>`).
- A valid Bob session / API key configured.
- Extra arguments can be forwarded via `--bob-args`.

## Input / output

- **Input:** extracted symbols (from [`doc_extractor`](../../extractor/doc_extractor/))
  plus their source snippets.
- **Output:** the documentation model augmented with a `block_diagram` (Mermaid) per symbol.

See [`docs/ZDOC.md`](../../docs/ZDOC.md) for the full specification, including the
AI Assisted Bob integration section.
