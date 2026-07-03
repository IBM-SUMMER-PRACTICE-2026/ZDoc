# asm_parser — Assembler parser

Parses Assembler source and extracts documented entry points, macros and labels
with their doc-comment blocks.

- **Layer:** `parser/`
- **Language / extensions:** Assembler (`.asm`, `.s`, `.mac`)
- **Planned binary:** `zdoc-asm-parser`
- **Status:** Planned

## Input / output

- **Input:** one or more Assembler source files given as arguments.
- **Output:** the shared parser JSON on stdout (see [`../README.md`](../README.md)).

> Macro-heavy Assembler may need pre-expansion for accurate parsing; ZDoc processes
> the raw source by default (see docs/ZDOC.md -> "Limitations").

## Build & run

```sh
make                      # builds ./zdoc-asm-parser
./zdoc-asm-parser file.asm [more files...]
make test
```

See [`docs/ZDOC.md`](../../docs/ZDOC.md) for the full specification.
