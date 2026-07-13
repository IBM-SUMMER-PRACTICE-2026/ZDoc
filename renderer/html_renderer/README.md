# html_renderer — HTML output renderer

Renders the documentation model as a single self-contained `index.html` with embedded
CSS and JavaScript — no external dependencies required for offline output.

- **Layer:** `renderer/`
- **Binary:** `zdoc-html-renderer`
- **Status:** Implemented

## Input / output

- **Input:** the normalised documentation-model JSON from
  [`doc_extractor`](../../extractor/doc_extractor/), read from stdin or a file.
- **Output:** `index.html` (and any assets) written under the output directory
  (default `./zdoc-out`).

Module nodes are expandable via `<details>`/`<summary>`. Block diagrams (AI Assisted
mode) render via the Mermaid JS library (bundled or CDN). For fully static offline
HTML without JS, diagrams are omitted and a note is inserted instead (see docs/ZDOC.md
-> "Limitations").

## Build & run

```sh
make                      # builds ./zdoc-html-renderer
... | ./zdoc-html-renderer --out-dir ./zdoc-out
make test                 # or: sh tests/run_tests.sh
```

Options: `--out-dir DIR`, `--title TITLE`, `--version`, `--help`; reads the model
JSON from a file argument or stdin. Exit codes: 0 ok, 1 bad input or write
failure, 2 usage error. The JSON contract, including the optional
`doc.diagram`/`doc.refs` keys, is documented by [`sample.json`](sample.json).

See [`docs/ZDOC.md`](../../docs/ZDOC.md) for the full specification.
