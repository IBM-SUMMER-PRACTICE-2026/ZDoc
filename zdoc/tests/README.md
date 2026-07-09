# zdoc CLI tests

Golden-file tests for the CLI's dry-run output. The CLI resolves options and
prints a fixed configuration block plus the discovered file list, so its output
is deterministic and can be locked with golden files.

## Run

```sh
sh run.sh            # check against expected/*.txt
sh run.sh update     # regenerate expected/*.txt after an intended change
make test            # from ../ ; runs this suite
```

Run from Git Bash / a POSIX shell (uses `sh`, `mktemp`, `diff`). Override the
binary under test with `ZDOC=/path/to/zdoc sh run.sh`.

## Layout

| Path              | Purpose                                                       |
|-------------------|---------------------------------------------------------------|
| `cases/*.args`    | CLI flags for one run each; the fixture `src` dir is appended  |
| `expected/*.txt`  | Golden output (CR-stripped, sorted)                            |
| `run.sh`          | Builds the fixture, runs every case, diffs, checks exit codes  |

## Fixture (built fresh in a temp dir each run)

```
src/a.c
src/b.java
src/sub/c.plx
src/tests/skip.c
```

## Cases

| Case            | Flags                          | Exercises                          |
|-----------------|--------------------------------|------------------------------------|
| `defaults`      | (none)                         | defaults + non-recursive top level |
| `recursive`     | `--recursive`                  | full-tree discovery, all languages |
| `lang-c`        | `--recursive --lang c`         | language filter                    |
| `exclude-tests` | `--recursive --exclude **/tests/**` | exclude glob                  |
| `html-format`   | `--output-format html --recursive` | resolved-config formatting     |

Beyond the golden cases, `run.sh` also asserts exit codes (bad `--mode`,
unknown flag, missing source, `--version`, `--help`) and that a CLI flag
overrides a `zdoc.yaml` value.

## Notes

- Output is **sorted** before comparison because `fs_walk` returns files in OS
  directory order, which is not stable across machines.
- File paths in the output are rooted at the source's last path component
  (`src/...`), so they don't leak the temp directory — keeping goldens portable.
- `--help` prints `argv[0]` (a machine-specific path); it is checked by exit
  code only, never golden-compared.
