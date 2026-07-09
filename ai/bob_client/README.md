# bob_client — context closure, Bob CLI invocation & response sanitizing

Drives AI Assisted mode. For one symbol it (1) assembles just enough context —
the declarations the body actually references — into the snippet the skill
expects, (2) calls the Bob CLI to obtain a brief block diagram, and (3) sanitizes
Bob's response into an embeddable Mermaid `flowchart` block for the renderers.

- **Layer:** `ai/`
- **Kind:** library unit — no binary of its own. The AI-mode CLI and the daemon
  link these objects and supply the declarations and bodies.
  - `closure.c` / `closure.h` — context closure (declaration index + snippet builder)
  - `bob_client.c` / `bob_client.h` — Bob invocation + response sanitizing
  - `util.c` / `util.h` — arena, string builder, file slurp
- **Status:** Implemented. Verified by `make test` (`tests/test_closure.c`).

## Context closure

Sending Bob the whole file is wasteful and noisy; sending only the body loses the
names that make a diagram readable ("set init flag", not "update CBFLAGS"). The
closure threads that needle:

1. `bc_index_build()` builds a name → declaration hash index (open-addressing,
   FNV-1a). Each declaration is indexed under every identifier it introduces
   *and* every identifier tokenized from its text, so members and `BASED`
   pointers resolve even when a parser under-reports names. Case-folding matches
   the language (PL/X, PLAS, HLASM, Pascal fold; C/C++/Java don't).
2. `bc_extract_refs()` tokenizes the function body into the identifiers it
   references, minus that language's keywords. Over-collection is intended.
3. `bc_closure()` looks each ref up and gathers the matching declarations under a
   character budget, in tiers: tier 0 = directly referenced, tier 1+ =
   declarations *those* reference (`transitive_depth`). Tier 0 is admitted first
   so it is never crowded out, and when context exists at least one declaration
   is always sent.
4. `bc_build_snippet()` emits exactly the skill's contract —
   `DOC` / `DECLARATIONS` / `CALLEES` / `FUNCTION (<lang>)` — omitting empty
   sections.

The result composes directly with `bob_diagram()` (next section): the closure
produces the `--snippet`, `bc_lang_display()` produces the `--lang`.

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

Closure and invocation compose end to end:

```c
bc_lang lang = bc_lang_parse("plx");

bc_index *idx = bc_index_build(decls, ndecls, lang);       /* once per module */
size_t nc = 0;
const bc_decl **c = bc_closure(body, idx, lang, 4000, 1, &nc);
char *snippet = bc_build_snippet(doc_brief, c, nc,
                                 callee_lines, ncallees, lang, body);

BobConfig cfg = bob_config_default();                      /* bob on PATH */
bob_annotate(&cfg, bc_lang_display(lang), snippet, sym);   /* -> sym->diagram */

free(snippet); free((void *)c);   /* bc_closure array is caller-freed */
bc_index_free(idx);
```

`bob_diagram()` (which `bob_annotate()` wraps) returns a fence-less Mermaid
flowchart that the caller frees, or NULL on any failure (bob missing, non-zero
exit, empty output, no flowchart). `bob_annotate()` stores that string into
`Symbol.diagram`.

## Requirements

- Bob CLI installed and on `PATH` (override with `--bob-cli <path>`).
- A valid Bob session / API key configured.
- Extra arguments can be forwarded via `--bob-args`.

## Input / output

- **Input:** for each symbol, its body text plus the module's declaration pool
  (name(s) + verbatim text) and any documented callee lines. The caller (AI-mode
  CLI / daemon) supplies these from the parsed `Module`; the closure selects what
  Bob actually needs.
- **Output:** the documentation model augmented with a Mermaid `diagram` per
  symbol (`Symbol.diagram`).

See [`docs/ZDOC.md`](../../docs/ZDOC.md) for the full specification, including the
AI Assisted Bob integration section, and [`../AI-FRONT-NOTES.md`](../AI-FRONT-NOTES.md)
for testing and remaining work.
