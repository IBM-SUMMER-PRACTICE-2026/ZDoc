# plx_parser — PL/X & PLAS parser

Parses PL/X and PLAS source and extracts documented procedures, entries, macros
and declarations together with their doc-comment blocks.

- **Layer:** `parser/`
- **Languages / extensions:** PL/X (`.plx`, `.pls`), PLAS (`.plas`)
- **Planned binary:** `zdoc-plx-parser`
- **Status:** Planned

## Input / output

- **Input:** one or more PL/X / PLAS source files given as arguments.
- **Output:** the shared parser JSON on stdout (see [`../README.md`](../README.md)).

The PL/X doc-comment block convention this parser must recognise is specified in
[`docs/plx-doccomment-convention.md`](../../docs/plx-doccomment-convention.md).
Example sources live at [`docs/student_grades.plx`](../../docs/student_grades.plx) and
[`docs/student_grades.plxmac`](../../docs/student_grades.plxmac).

## Build & run

```sh
make                      # builds ./zdoc-plx-parser
./zdoc-plx-parser file.plx [more files...]
make test
```

See [`docs/ZDOC.md`](../../docs/ZDOC.md) for the full specification.
