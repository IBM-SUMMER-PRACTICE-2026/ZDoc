# md_renderer — Markdown output renderer

Renders the documentation model as Markdown: one `.md` file per module plus a root
`index.md` carrying the module tree. GitHub / GitLab / Obsidian compatible.

- **Layer:** `renderer/`
- **Planned binary:** `zdoc-md-renderer`
- **Status:** Planned

## Input / output

- **Input:** the daemon's already-walked module_tree tables (`modtree_dir_table_t`/
  `modtree_file_table_t`) plus its parsed `Module` array, passed directly in memory -
  no JSON, no doc_extractor stage in between. See `src/md_renderer.h`.
- **Output:** Markdown files written under the output directory (default `./zdoc-out`).

Block diagrams (AI Assisted mode) are rendered as fenced ` ```mermaid ``` ` blocks.
Module nodes use `<details>`/`<summary>` so they stay collapsible on GitHub. See the
Markdown output example in [`docs/ZDOC.md`](../../docs/ZDOC.md).

## Build & run

```sh
make                      # builds ./zdoc-md-renderer
... | ./zdoc-md-renderer --out-dir ./zdoc-out
make test
```

See [`docs/ZDOC.md`](../../docs/ZDOC.md) for the full specification.
