# zdoc CLI tests

Smoke tests for the CLI frontend. They exercise the interface only -
option parsing, config loading, validation, and the request JSON - so the
suite passes without the daemon or any stage binaries.

## Run

```sh
sh run_tests.sh      # from this directory or via `make test` in ../
```

Run from Git Bash / a POSIX shell. `make test` from cmd/PowerShell falls back
to a bare `--version` check.

## What is covered

- `--version`, `--help`, and the bare-`zdoc` about page
- exit codes: unknown option, missing source path, bad `--mode`,
  nonexistent input, unknown `--lang` value
- language canonicalization (`--lang assembler` -> `"asm"`)
- request JSON content (`--output-format`, `--lang` comma lists)
- `zdoc.json` config loading (scalars + lists) in a temp directory
