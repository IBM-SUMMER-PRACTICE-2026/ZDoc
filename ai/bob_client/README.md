# bob_client — AI diagram generation (`zdoc-bob-client`)

Drives AI Assisted mode. A standalone JSON-in → JSON-out filter: it reads the
shared parser JSON (produced with `--ai-context`), calls the Bob CLI once per
function/procedure/entry symbol, and re-emits the same JSON augmented with a
validated Mermaid `block_diagram` (plus harvested `call_edges`) per symbol.

- **Layer:** `ai/`
- **Binary:** `zdoc-bob-client`
- **Spec:** [`docs/zdoc-ai-mode.md`](../../docs/zdoc-ai-mode.md) (authoritative)

## Design highlights

- **Structured contract** — the shipped Bob skill
  ([`.bob/skills/zdoc-diagram/`](../../.bob/skills/zdoc-diagram/)) returns a
  small JSON graph; `graph.c` validates it mechanically and serializes the
  Mermaid itself (escaping is code, not model behavior). A bare
  `flowchart TD` block is accepted as fallback.
- **Content-addressed cache** — `SHA-256(snippet ‖ skill version ‖ command)`
  → cached graph. Unchanged functions never hit Bob again; interrupted runs
  resume for free. `--ai-cache-dir`, `--ai-no-cache`, `--ai-refresh`.
- **Worker pool** — `--ai-jobs N` (default 4) concurrent Bob calls;
  deterministic output order regardless of completion order.
- **Closure assembly** (`closure.c`) — per symbol, only the declarations the
  body references (tiered, budgeted, case-folded per language), the symbol's
  own doc brief, and one-line signatures of extracted callees.
- **Never abort** — per-symbol retry once, then `diagram_error` + a line in
  `zdoc-ai-failures.log`; run-end stats on stderr.

## Files

| File | Role |
|------|------|
| `closure.c/.h` | Declaration index, reference extraction, tiered closure, snippet builder |
| `graph.c/.h` | Graph JSON parse, validation, Mermaid serialization, normalization/repair, mermaid fallback parse, call harvesting |
| `bob_client.c/.h` | Provider spawn (posix_spawn, stdin, timeout), retry policy, cache, failure log |
| `json.c/.h` | Vendored minimal JSON reader/writer (no external deps) |
| `sha256.c/.h` | Vendored SHA-256 for cache keys |
| `main.c` | The `zdoc-bob-client` filter |
| `tests/` | Unit tests (closure, graph), stub-CLI process tests, e2e fixture run |

## Usage

```sh
zdoc-c-parser --ai-context src/*.c | zdoc-bob-client --bob-cli bob > augmented.json
```

Key options: `--input <file>`, `--bob-cli <path>`, `--bob-args "<args>"`,
`--ai-jobs N`, `--ai-timeout SECS`, `--ai-cache-dir <dir>`, `--ai-no-cache`,
`--ai-refresh`, `--fail-log <path>`, `--ai-record <dir>`.

## Build & test

```sh
make        # builds ./zdoc-bob-client
make test   # unit tests + stub-CLI process tests + e2e fixture snapshot
```

In offline mode (the default) this binary is never invoked.
