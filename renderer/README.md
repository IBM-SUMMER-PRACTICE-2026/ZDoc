# renderer/ — output renderers

The final stage. Renderers consume the normalised documentation model from
[`extractor/doc_extractor`](../extractor/doc_extractor/) (optionally augmented with
Bob block diagrams from [`ai/bob_client`](../ai/bob_client/)) and write documentation
to the output directory.

| Renderer                        | Output                                             |
|---------------------------------|----------------------------------------------------|
| [`md_renderer`](md_renderer/)   | One `.md` per module + a root `index.md` tree       |
| [`html_renderer`](html_renderer/) | A single self-contained `index.html`              |

Both render the module tree as expandable nodes and each symbol's signature, brief,
parameters, returns, notes, block diagram (AI mode) and cross-references. Block
diagrams are emitted as fenced ` ```mermaid ``` ` blocks (Markdown) or via the
Mermaid JS library (HTML).

See [`docs/ZDOC.md`](../docs/ZDOC.md) for the full specification and output examples.
