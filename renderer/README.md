# renderer/ — output renderers

The final stage. Renderers consume the daemon's already-walked module_tree tables
and parsed `Module` array directly (optionally augmented with Bob block diagrams
from [`ai/bob_client`](../ai/bob_client/)) and write documentation to the output
directory. There used to be a `doc_extractor` stage in between that copied this
same data into its own model before handing it to the renderers; that copy added
no real value once every parser already emits this same shared `Module`/`Symbol`
shape, so each renderer now reads it directly and does its own small per-file
language lookup and module matching (duplicated between the two, on purpose -
see each renderer's own header for why).

| Renderer                        | Output                                             |
|---------------------------------|----------------------------------------------------|
| [`md_renderer`](md_renderer/)   | One `.md` per module + a root `index.md` tree       |
| [`html_renderer`](html_renderer/) | A single self-contained `index.html`              |

Both render the module tree as expandable nodes and each symbol's signature, brief,
parameters, returns, notes and block diagram (AI mode). Block diagrams are emitted
as fenced ` ```mermaid ``` ` blocks (Markdown) or via the Mermaid JS library (HTML).

See [`docs/ZDOC.md`](../docs/ZDOC.md) for the full specification and output examples.
