# parser/ — language parsers

Each parser is a **standalone C11 executable** that reads one or more source files
and prints a single JSON document on stdout. Downstream ZDoc stages (the
[`extractor`](../extractor/), [`renderer`](../renderer/) and
[`ai/bob_client`](../ai/bob_client/)) consume that JSON — parsers never write output
files themselves.

| Parser                        | Languages   | Extensions                              |
|-------------------------------|-------------|-----------------------------------------|
| [`plx_parser`](plx_parser/)   | PL/X, PLAS  | `.plx` `.pls` `.plas`                    |
| [`c_parser`](c_parser/)       | C, C++      | `.c` `.h` `.cpp` `.cxx` `.cc` `.hpp`     |
| [`java_parser`](java_parser/) | Java        | `.java`                                  |
| [`asm_parser`](asm_parser/)   | Assembler   | `.asm` `.s` `.mac`                       |
| [`pascal_parser`](pascal_parser/) | Pascal  | `.pas` `.pp`                             |

## Shared JSON contract

Every parser emits the same shape so the rest of the pipeline stays
language-agnostic. `module_doc` and each symbol's `doc` are optional. Note
there is no per-module `"language"` field - the extractor already knows each
file's language from its extension before it ever invokes the parser, so the
parser doesn't need to echo it back.

```json
{
  "zdoc_parser": "<lang>",
  "version": "<semver>",
  "modules": [
    {
      "filename": "path/to/source.ext",
      "module_doc": { "brief": "…", "params": [], "returns": "…", "notes": "…" },
      "symbols": [
        {
          "kind": "function|prototype|macro|type|variable|procedure|entry",
          "line": 42,
          "name": "SYMBOL",
          "signature": "whitespace-collapsed, comment-free signature",
          "doc": {
            "brief": "one-line description",
            "params": [ { "name": "P", "desc": "…" } ],
            "returns": "…",
            "notes": "…"
          }
        }
      ]
    }
  ]
}
```

On a per-file parse error, still emit that file's entry with `"error": true` and an
empty `symbols` array, and exit non-zero — the pipeline should degrade gracefully.

## Conventions

- Binary name: `zdoc-<lang>-parser` (e.g. `zdoc-c-parser`).
- Build: portable Makefile — `CC ?= cc`, `CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Wpedantic`,
  with `all` / `test` / `clean` targets.
- Prefix public API symbols per module (e.g. `cp_` for `c_parser`).
- Ship a `tests/` directory with representative sample sources.

See [`docs/ZDOC.md`](../docs/ZDOC.md) for the full specification.
