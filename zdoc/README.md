# zdoc — CLI entry point

The user-facing command that orchestrates the whole pipeline. It resolves options and
config, discovers source files, dispatches each to the right [parser](../parser/),
pipes the result through the [doc_extractor](../extractor/doc_extractor/), optionally
calls [bob_client](../ai/bob_client/) (AI mode), and drives the selected
[renderer](../renderer/).

- **Layer:** CLI (top of the tree)
- **Planned binary:** `zdoc`
- **Status:** Planned

## Pipeline

```
source files → <lang>_parser → doc_extractor → [bob_client (--mode ai)] → md_renderer | html_renderer → out-dir
```

The parser is chosen per file by extension (see the table in
[`../parser/README.md`](../parser/README.md)).

## Usage

```
zdoc [options] <source_dir_or_file> [<source_dir_or_file> ...]
```

Key options (full list in [`docs/ZDOC.md`](../docs/ZDOC.md) -> "CLI Usage"):

| Option                   | Description                                   |
|--------------------------|-----------------------------------------------|
| `--mode offline\|ai`     | Operating mode (default `offline`)            |
| `--output-format md\|html` | Output format (default `md`)                |
| `--out-dir <path>`       | Output directory (default `./zdoc-out`)       |
| `--lang <lang>[,<lang>]` | Restrict to listed languages                  |
| `--recursive`            | Recurse into subdirectories                   |
| `--exclude <glob>`       | Exclude matching files                         |
| `--bob-cli <path>`       | Path to the Bob CLI binary                     |
| `--title <string>`       | Project title shown in the output             |

Configuration may also be supplied via `zdoc.yaml` / `zdoc.json`
(see [`../zdoc.yaml.example`](../zdoc.yaml.example)); command-line options override it.

## Build & run

```sh
make                      # builds ./zdoc
./zdoc --output-format html ./src
make test
```

See [`docs/ZDOC.md`](../docs/ZDOC.md) for the full specification.
