# java_parser — Java parser

Parses Java source and extracts documented classes, methods and declarations with
their doc-comment blocks.

- **Layer:** `parser/`
- **Language / extensions:** Java (`.java`)
- **Binary:** `zdoc-java-parser`
- **Status:** 🚧 In progress — implemented on branch [`2-feature-java-parser`](https://github.com/IBM-SUMMER-PRACTICE-2026/ZDoc/tree/2-feature-java-parser).

> This directory is a placeholder in the scaffold. The working implementation
> (`Makefile`, `java_parser.c/.h`, `doc_comment.c/.h`, `util.h`, `main.c`) lands here
> when branch `2-feature-java-parser` is merged. Don't add stub sources that would
> conflict with that merge.

## Input / output

- **Input:** one or more `.java` source files given as arguments.
- **Output:** the shared parser JSON on stdout (see [`../README.md`](../README.md)).

See [`docs/ZDOC.md`](../../docs/ZDOC.md) for the full specification.
