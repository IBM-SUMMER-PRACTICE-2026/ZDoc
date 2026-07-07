# bob_client — Bob CLI invocation & response parsing

Drives AI Assisted mode. For each extracted symbol it calls the Bob CLI to obtain a
brief (detailed-design level) block diagram of the function body, then parses Bob's
response into a Mermaid flowchart block for the renderers to embed.

- **Layer:** `ai/`
- **Planned binary:** `zdoc-bob-client`
- **Status:** Planned

## Bob invocation

```
bob explain --diagram --brief --lang <detected_language> --snippet "<function_source>"
```

- Bob returns one Mermaid `flowchart` block per symbol.
- Diagrams are **brief** — one box per logical step, not per source line.

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
