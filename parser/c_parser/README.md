# c_parser — C & C++ parser

Parses C and C++ source and extracts documented functions, prototypes, macros,
types and variables with their Doxygen-style doc comments.

- **Layer:** `parser/`
- **Languages / extensions:** C (`.c`, `.h`), C++ (`.cpp`, `.cxx`, `.cc`, `.hpp`, …)
- **Binary:** `zdoc-c-parser`
- **Status:** 🚧 In progress — implemented on branch [`C0parser`](https://github.com/IBM-SUMMER-PRACTICE-2026/ZDoc/tree/C0parser).

> This directory is a placeholder in the scaffold. The working implementation
> (`Makefile`, `c_parser.c/.h`, `main.c`, `tests/`) lands here when branch
> `C0parser` is merged. Don't add stub sources that would conflict with that merge.

## Input / output

- **Input:** one or more C/C++ source files given as arguments.
- **Output:** the shared parser JSON on stdout (see [`../README.md`](../README.md)).

See [`docs/ZDOC.md`](../../docs/ZDOC.md) for the full specification.
