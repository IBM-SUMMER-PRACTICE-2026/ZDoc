# pascal_parser — Pascal parser

Parses Pascal source and extracts documented procedures, functions, units and
declarations with their doc-comment blocks.

- **Layer:** `parser/`
- **Language / extensions:** Pascal (`.pas`, `.pp`)
- **Planned binary:** `zdoc-pascal-parser`
- **Status:** Planned

## Input / output

- **Input:** one or more Pascal source files given as arguments.
- **Output:** the shared parser JSON on stdout (see [`../README.md`](../README.md)).

## Build & run

```sh
make                      # builds ./zdoc-pascal-parser
./zdoc-pascal-parser file.pas [more files...]
make test
```

See [`docs/ZDOC.md`](../../docs/ZDOC.md) for the full specification.
