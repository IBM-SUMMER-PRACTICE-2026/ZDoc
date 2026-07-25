# ZDoc

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Language: C](https://img.shields.io/badge/language-C-00599C.svg)](zdoc)
[![Prebuilt binaries](https://img.shields.io/badge/binaries-ZDoc--releases-blue.svg)](https://github.com/IBM-SUMMER-PRACTICE-2026/ZDoc-releases)

There is effectively no actively maintained, open-source documentation generator for
**PL/X**, **PLAS**, or mainframe **Assembler** — ZDoc is one. It also handles **C**,
**C++**, **Java**, and **Pascal** in the same tool, extracting per-symbol
documentation (signature, parameters, returns, cross-references) straight from doc
comments in the source and rendering it as browsable Markdown or a self-contained
HTML site — in the spirit of Doxygen or JavaDoc, but built for mixed legacy/modern
mainframe codebases. An optional **AI Assisted** mode calls the Bob CLI to add a brief
Mermaid block diagram for each documented function.

## Quick look

This is real, unedited output from the current build — no mockup. The input is an
actual procedure from [`docs/student_grades.plx`](docs/student_grades.plx), written in
the mainframe-style banner-comment convention documented in
[`docs/plx-doccomment-convention.md`](docs/plx-doccomment-convention.md):

```plx
/*  Title: ADD_STUDENT:                                         @L0A*/
/*                                                              @L0A*/
/*  Logic: Add a new student to the system                      @L0A*/
/*                                                              @L0A*/
/*  Input: ID, Name, Year                                       @L0A*/
/*         Where ID is:   1) Fixed(31) number                   @L0A*/
/*         Where Name is: 1) A character array, max length 30   @L0A*/
/*         Where Year is: 1) Fixed(31) number                   @L0A*/
/*                                                              @L0A*/
/*  Output: Return code - 0  - RC_SUCCESS                       @L0A*/
/*                        12 - RC_ARRAY_FULL                    @L0A*/
/********************************************************************/
ADD_STUDENT: PROC(ID, NAME, YEAR) RETURNS(FIXED(31));
```

```sh
zdoc --mode offline --output-format md --title "Student Grades" ./docs
```

produces:

````md
<details>
<summary><strong>ADD_STUDENT</strong> — Add a new student to the system</summary>

**Signature**
```plx
ADD_STUDENT: PROC(ID, NAME, YEAR) RETURNS(FIXED(31));
```

**Parameters**

| Name | Description |
|------|-------------|
| ID | Fixed(31) number |
| Name | A character array with maximum length of 30 |
| Year | Fixed(31) number |

**Returns**
Return code - 0  - RC_SUCCESS 12 - RC_ARRAY_FULL

</details>
````

The same offline path works today for C and Java sources too (Doxygen-style `/** */`
comments) — see [Supported languages](#supported-languages) for what's next.

## Why ZDoc

| | ZDoc | Doxygen | JavaDoc |
|---|---|---|---|
| PL/X, PLAS, mainframe Assembler | ✅ | ❌ | ❌ |
| C, C++, Java, Pascal | ✅ | C/C++ (+ more via config) | Java only |
| Offline by default, no network calls | ✅ | ✅ | ✅ |
| AI-assisted design diagrams | ✅ `--mode ai`, Mermaid | ❌ | ❌ |
| Output formats | Markdown + self-contained HTML | HTML, LaTeX, man, … | HTML |
| License | MIT | GPL-2.0 | JDK-bundled |

> **Specification:** [`docs/ZDOC.md`](docs/ZDOC.md) is the source of truth for this
> project (see [`AGENTS.md`](AGENTS.md)). If anything here conflicts with `/docs`, `/docs` wins.

## Contents

- [Quick look](#quick-look)
- [Why ZDoc](#why-zdoc)
- [Supported languages](#supported-languages)
- [Operating modes](#operating-modes)
- [Installation](#installation)
- [Quick start](#quick-start)
- [Configuration file](#configuration-file)
- [Repository layout](#repository-layout)
- [Building from source](#building-from-source)
- [Components](#components)
- [License](#license)

## Supported languages

| Language   | File extensions               |
|------------|-------------------------------|
| PL/X       | `.plx`, `.pls`                |
| PLAS       | `.plas`                       |
| C          | `.c`, `.h`                    |
| C++        | `.cpp`, `.cxx`, `.cc`, `.hpp`, `.c++` |
| Java       | `.java`                       |
| Assembler  | `.asm`, `.s`, `.mac`          |
| Pascal     | `.pas`, `.pp`                 |

## Operating modes

- **Offline** (default) — parse source and extract documentation with no external calls.
  Suitable for air-gapped environments, CI pipelines, and quick local runs.
- **AI Assisted** (`--mode ai`) — additionally calls the Bob CLI per function to generate
  a brief Mermaid block diagram, inserted directly into that function's documentation
  section. Requires the Bob CLI on `PATH` and a valid session/API key.

See [`docs/ZDOC.md`](docs/ZDOC.md) for the full behavior of each mode.

## Installation

Prebuilt binaries for Linux, macOS, and Windows (x64 and arm64) are published to
[IBM-SUMMER-PRACTICE-2026/ZDoc-releases](https://github.com/IBM-SUMMER-PRACTICE-2026/ZDoc-releases)
on every tagged release. Install the right one for your machine with:

```sh
curl -fsSL https://raw.githubusercontent.com/IBM-SUMMER-PRACTICE-2026/ZDoc/main/scripts/install.sh | sh
```

This detects your OS/architecture, downloads the matching `zdoc` binary, and puts it on
`PATH`. See [`scripts/install.sh`](scripts/install.sh) for supported environment
variables (`ZDOC_VERSION`, `ZDOC_INSTALL_DIR`, etc.) to pin a version or install
location.

Alternatively, [build from source](#building-from-source).

## Quick start

```sh
# Offline Markdown documentation for a directory, recursively
zdoc --mode offline --output-format md --recursive ./src

# AI Assisted HTML documentation, with a custom title
zdoc --mode ai --output-format html --out-dir ./docs --title "My Project" ./src

# Single file, offline, HTML
zdoc --output-format html ./src/mymodule.plx
```

Run `zdoc --help` for the full option list, or see
[`docs/ZDOC.md`](docs/ZDOC.md#cli-usage) for every flag with examples.

## Configuration file

ZDoc reads an optional `zdoc.yaml` (or `zdoc.json`) from the working directory; CLI
options always override it. See [`zdoc.yaml.example`](zdoc.yaml.example):

```yaml
title: "My Project Documentation"
mode: offline                  # offline | ai
output_format: html            # md | html
out_dir: ./docs
recursive: true
languages:
  - plx
  - c
  - assembler
exclude:
  - "**/*.test.c"
  - "**/test/**"
bob_cli: bob
bob_args: "--session default"
```

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
├── ai-bob/              — Bob CLI invocation and response parsing
├── renderer/
│   ├── md_renderer      — Markdown output renderer
│   └── html_renderer    — HTML output renderer
└── zdoc                 — CLI entry point (daemon + CLI front end)
```

Each component directory has a `README.md` describing its purpose, its input/output
contract, and its build. The shared parser JSON contract is documented in
[`parser/README.md`](parser/README.md).

## Building from source

Every component builds its own executable via its own Makefile. The top-level
`Makefile` fans out into each component that has one:

```sh
make          # build every component that has a Makefile
make test     # run each component's tests
make clean    # remove build artifacts
make dist     # build everything and collect release artifacts into ./dist
make list     # show which components are currently wired into the build
```

To build a single component:

```sh
make -C parser/c_parser
```

## Components

| Component                | Path                       | Status                                        |
|---------------------------|----------------------------|------------------------------------------------|
| PL/X + PLAS parser        | `parser/plx_parser`        | In progress                                     |
| C / C++ parser            | `parser/c_parser`          | In progress                                     |
| Java parser               | `parser/java_parser`       | In progress                                     |
| Assembler parser          | `parser/asm_parser`        | Planned                                         |
| Pascal parser             | `parser/pascal_parser`     | Planned                                         |
| Doc extractor (shared)    | `extractor/doc_extractor`  | In progress                                     |
| Bob CLI client            | `ai-bob`                   | In progress                                     |
| Markdown renderer         | `renderer/md_renderer`     | In progress                                     |
| HTML renderer             | `renderer/html_renderer`   | In progress                                     |
| CLI entry point           | `zdoc`                     | In progress                                     |

Releases are tag-driven: pushing a `v<major>.<minor>.<patch>` tag on `main` builds every
platform binary and publishes it to
[ZDoc-releases](https://github.com/IBM-SUMMER-PRACTICE-2026/ZDoc-releases) via
[`.github/workflows/release.yml`](.github/workflows/release.yml).

## License

ZDoc is released under the [MIT License](LICENSE).
