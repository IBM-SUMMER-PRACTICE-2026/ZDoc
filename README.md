# ZDoc

ZDoc is a documentation generation tool designed to extract and present structured documentation from source files written in PL/X, PLAS, C, C++, Java, Assembler, and Pascal. It operates similarly to Doxygen or JavaDoc, but is purpose-built for mainframe and mixed-language codebases.

> **Specification:** [`docs/ZDOC.md`](docs/ZDOC.md) is the source of truth for this
> project (see [`AGENTS.md`](AGENTS.md)). If anything here conflicts with `/docs`, `/docs` wins.

## Repository layout

ZDoc is built as a set of small, standalone C components. Each parser reads source
files and emits JSON on stdout; downstream stages consume that JSON and render the
final Markdown or HTML documentation.

```
zdoc
├── parser/
│   ├── plx_parser       — PL/X and PLAS parser
│   ├── c_parser         — C and C++ parser
│   ├── java_parser      — Java parser
│   ├── asm_parser       — Assembler parser
│   └── pascal_parser    — Pascal parser
├── extractor/
│   └── doc_extractor    — Comment block and tag extractor (shared)
├── ai/
│   └── bob_client       — Bob CLI invocation and response parsing
├── renderer/
│   ├── md_renderer      — Markdown output renderer
│   └── html_renderer    — HTML output renderer
└── zdoc                 — CLI entry point
```

Each component directory has a `README.md` describing its purpose, its input/output
contract, and its build. The shared parser JSON contract is documented in
[`parser/README.md`](parser/README.md).

## Building

Every component builds its own `zdoc-<module>` executable via its own Makefile. The
top-level `Makefile` fans out into each component that has one:

```sh
make          # build every component that has a Makefile
make test     # run each component's tests
make clean    # remove build artifacts
make list     # show which components are currently wired into the build
```

To build a single component:

```sh
make -C parser/c_parser
```

## Components

| Component                | Path                     | Status                                    |
|--------------------------|--------------------------|-------------------------------------------|
| PL/X + PLAS parser       | `parser/plx_parser`      | Planned                                   |
| C / C++ parser           | `parser/c_parser`        | In progress — branch `C0parser`           |
| Java parser              | `parser/java_parser`     | In progress — branch `2-feature-java-parser` |
| Assembler parser         | `parser/asm_parser`      | Planned                                   |
| Pascal parser            | `parser/pascal_parser`   | Planned                                   |
| Doc extractor (shared)   | `extractor/doc_extractor`| Planned                                   |
| Bob CLI client           | `ai/bob_client`          | Planned                                   |
| Markdown renderer        | `renderer/md_renderer`   | Planned                                   |
| HTML renderer            | `renderer/html_renderer` | Planned                                   |
| CLI entry point          | `zdoc`                   | Planned                                   |

## Operating modes

- **Offline** (default) — parse source and extract documentation with no external calls.
- **AI Assisted** (`--mode ai`) — additionally call the Bob CLI per symbol to generate a
  brief Mermaid block diagram.

See [`docs/ZDOC.md`](docs/ZDOC.md) for CLI usage, options, output formats, and examples.

## License

See [`LICENSE`](LICENSE).
