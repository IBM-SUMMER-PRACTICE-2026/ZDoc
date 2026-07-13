# zdoc - CLI entry point

The user-facing command. It resolves options and config (defaults <
`zdoc.yaml`/`zdoc.json` < command-line flags), validates the inputs, and
formats a `generate` request describing the run. The pipeline itself -
[parsers](../parser/), [doc_extractor](../extractor/doc_extractor/),
[bob_client](../ai/bob_client/) (AI mode) and the [renderers](../renderer/) -
is owned by the zdoc daemon (in development on another branch); the CLI's job
ends at handing it a well-formed request.

- **Layer:** CLI (top of the tree)
- **Binary:** `zdoc`
- **Status:** In progress - option/flag parsing, `zdoc.yaml` + `zdoc.json`
  loading, language validation and input validation are implemented. The
  resolved request JSON is printed to stdout as a stand-in until the daemon
  transport (TCP / named pipe / stdio) is decided; a `daemon.c` client module
  will then ship the same payload instead.

## Pipeline

```
zdoc (this CLI) -> request JSON -> zdoc daemon -> <lang>_parser -> doc_extractor -> [bob_client (--mode ai)] -> md_renderer | html_renderer -> out-dir
```

The parser is chosen per file by extension (see the table in
[`../parser/README.md`](../parser/README.md)). File discovery, exclude-glob
matching and parser dispatch happen daemon-side.

## Source layout

| File                      | Purpose                                                      |
|---------------------------|--------------------------------------------------------------|
| `options.h` / `options.c` | `zd_options` model, defaults, language table (names+aliases) |
| `cli.h` / `cli.c`         | Command-line parsing, `--help` / `--version` / about page    |
| `config.h` / `config.c`   | `zdoc.yaml` (subset) and `zdoc.json` loaders                 |
| `request.h` / `request.c` | Input validation + `generate` request JSON formatting        |
| `main.c`                  | Wiring: defaults -> config -> args -> validate -> request        |

Precedence: **defaults < `./zdoc.yaml` (or `./zdoc.json`) < command-line flags.**

## Usage

```
zdoc [options] <source_dir> [<source_dir> ...]
```

Bare `zdoc` prints an about page. Key options (full list in
[`docs/ZDOC.md`](../docs/ZDOC.md) -> "CLI Usage"):

| Option                   | Description                                   |
|--------------------------|-----------------------------------------------|
| `--mode offline\|ai`     | Operating mode (default `offline`)            |
| `--output-format md\|html` | Output format (default `md`)                |
| `--out-dir <path>`       | Output directory (default `./zdoc-out`)       |
| `--lang <lang>[,<lang>]` | Restrict to listed languages                  |
| `--recursive`            | Recurse into subdirectories                   |
| `--exclude <glob>`       | Exclude matching files (repeatable)           |
| `--bob-cli <path>`       | Path to the Bob CLI binary                    |
| `--title <string>`       | Project title shown in the output             |

Supported languages: `plx`, `plas`, `c`, `cpp` (`c++`), `java`, `asm`
(`assembler`), `pascal`. Configuration may also be supplied via `zdoc.yaml` /
`zdoc.json` (see [`../zdoc.yaml.example`](../zdoc.yaml.example)); command-line
options override it.

## Build & run

```sh
make                      # builds ./zdoc
./zdoc --output-format html ./src
make test                 # smoke tests (POSIX shell; Git Bash on Windows)
make install              # copies zdoc to ~/bin (override: make install PREFIX=...)
```

See [`docs/ZDOC.md`](../docs/ZDOC.md) for the full specification.
