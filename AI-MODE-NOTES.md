# ZDoc AI Mode — everything there is to know

*Compiled 2026-07-09 from the authoritative spec (`docs/zdoc-ai-mode.md`), the
shipped skill (`.bob/skills/zdoc-diagram/`), the implementation
(`ai/bob_client/`), and the optimization discussion recorded in
`NEXT-STEPS.md`. Companion to that file: NEXT-STEPS says what to do next;
this document says what exists and why. All referenced code lives on the
`feature/ai-skill-context` branch (commit `50516b8`) until the Phase 1 PR
lands.*

---

## 1. What "the AI" is in this project

ZDoc's AI Assisted mode (`--mode ai`) calls the **IBM Bob CLI** — an
agentic AI command-line tool — once per extracted function/procedure/entry
symbol to obtain a **brief block diagram** of that symbol's logic. The
diagram lands in the generated documentation as a Mermaid `flowchart TD`.

Two things are deliberately true about the integration:

- **The AI is a replaceable provider, not a dependency.** The invocation is
  a config-driven command template (`bob explain --diagram --brief --lang
  {lang}` by default, overridable via `--ai-command`). Nothing in the code
  knows anything about Bob beyond "spawn this argv, write a snippet to its
  stdin, read text from its stdout."
- **Offline mode is untouched.** The filter simply isn't in the pipe. The
  parsers stay offline, deterministic, and network-free by design; AI access
  exists in exactly one binary.

### Pipeline position

```
source → <lang>_parser --ai-context → doc_extractor → zdoc-bob-client → renderer
```

`zdoc-bob-client` is a standalone **JSON-in → JSON-out filter** (stdin/stdout
or `--input <file>`). It passes the parser JSON through unchanged and adds,
per symbol, one of:

| Field | Meaning |
|---|---|
| `block_diagram` | validated Mermaid body (string) |
| `call_edges` | extracted-symbol names the diagram calls (cross-reference feed) |
| `diagram_error: true` | generation failed after retry — renderer emits *"Block diagram unavailable."* |

---

## 2. The contracts (from the spec)

Three contracts hold the whole design together. All live in
`docs/zdoc-ai-mode.md`, which AGENTS.md makes the source of truth.

### 2.1 Parser contract: `--ai-context`

Without the flag, parser output is **byte-identical** to before (snapshot
tests depend on this). With it:

- each executable symbol additionally carries `"body"` (verbatim source) and
  `"line_end"`;
- each module carries `"declarations"`: `{ names: [...], line, text }`.

**The every-name rule:** a declaration is listed under *every* identifier the
parser knows it introduces — struct tag, typedef name, members, the PL/X
`BASED` pointer, HLASM DSECT fields, Pascal record fields. Parsers may
under-report exotic names because the consumer additionally indexes each
declaration under all identifiers tokenized from its `text` — lookups err
toward over-matching, which is harmless: precision comes from the reference
lookup, and the budget caps snippet size.

### 2.2 Graph contract: what Bob must return

Bob returns **one JSON object** (a single ```json fence is tolerated):

```json
{
  "nodes": [
    { "id": "A", "kind": "step",     "text": "Entry: INITPROC" },
    { "id": "B", "kind": "decision", "text": "Storage obtained?" },
    { "id": "C", "kind": "return",   "text": "Return RC=8 - storage failure" },
    { "id": "D", "kind": "call",     "text": "Call TERMPROC" }
  ],
  "edges": [
    { "from": "A", "to": "B" },
    { "from": "B", "to": "C", "label": "No" }
  ]
}
```

Validation is mechanical (`graph.c`): 1–14 nodes; unique non-empty ids;
`kind` ∈ {step, decision, call, return}; non-empty text; edges reference
existing ids; every node reachable from the first ("entry"); every out-edge
of a `decision` carries a label. A bare `flowchart TD` Mermaid block is
accepted as a **fallback** and parsed into the same graph model.

**Why a JSON graph instead of raw Mermaid** (the single most important
design decision): ZDoc validates the graph and serializes the Mermaid
*itself*, so syntax and escaping are deterministic code, not model behavior.
The model can never inject broken Mermaid, unescaped quotes, or markup into
the rendered docs — `text` is data, and the serializer quotes/escapes it
(`"` → `#quot;`, control chars dropped).

### 2.3 Snippet contract: what ZDoc sends Bob

Up to four sections, `FUNCTION` always last; empty sections omitted:

```
DOC:
<the symbol's own doc-comment brief>

DECLARATIONS:
<only the declarations the body references>

CALLEES:
TERMPROC: TERMPROC: PROC(ANCHOR) — Terminate subsystem

FUNCTION (<PL/X | PLAS | C | C++ | Java | HLASM | Pascal>):
<function body>
```

The snippet travels on **stdin**, never argv (argv has size limits, and no
shell is ever involved — that was a security decision: zero injection
surface).

---

## 3. The skill (`.bob/skills/zdoc-diagram/`, version 1.0.0)

The skill is the prompt-side half of the contract. Its contents:

- **`SKILL.md`** — the hard output contract (one JSON object, node kinds,
  single-letter ids in flow order, decision labels mandatory, 1–14 nodes,
  text under ~6 words, plain words only since ZDoc does all escaping), the
  degenerate-case rule (`"No executable logic"` single-node graph), and the
  definition of *brief*: one node per **logical step**, 5–12 nodes typical,
  merge straight-line sequences, decision nodes only for outcome-changing
  branches, loops as one body node + back-edge, never unrolled.
- **`conventions.md`** — label conventions: `Entry: NAME`, `Return RC=n -
  reason`, `Call TERMPROC` (exact callee name so ZDoc can cross-link),
  decisions phrased as questions.
- **`examples/`** — golden input→output pairs for PL/X, C, and HLASM, plus
  `granularity-negative.md` (a deliberately-wrong too-detailed answer and
  its correction). The PL/X and HLASM examples define the expected level of
  abstraction; the skill tells the model to follow them exactly.

The skill also explains the input sections to the model: trust `DOC:` for
intent, use `DECLARATIONS:` to *name things meaningfully* ("set init flag in
control block", not "update CBFLAGS") but never diagram the declarations
themselves, and emit a `call` node with the exact name for anything listed
in `CALLEES:`.

**The skill version is part of the cache key** (`BC_SKILL_VERSION`). Editing
the skill and bumping the version invalidates every cached diagram —
intentional, since a new prompt means potentially different output.

---

## 4. What was implemented, file by file, and why

All C11, **no external dependencies** (JSON reader/writer and SHA-256 are
vendored single-file implementations — a project constraint). ~2,300 lines
plus tests.

### `closure.c` — context selection

Sends Bob only what the function body actually references, because dumping
whole modules would blow context, cost, and focus.

1. **Reference extraction** (`bc_extract_refs`) — tokenize the body with
   `[A-Za-z_$#@][A-Za-z0-9_$#@]*` (the `$#@` matter for mainframe
   languages), subtract a small per-language keyword table (C, C++, Java,
   PL/X+PLAS, HLASM incl. `R0`–`R15`, Pascal). Results are sorted + deduped
   for determinism. Over-collection is intended; unresolved names cost
   nothing. The keyword tables are deliberately small — precision comes from
   the index lookup, not the tables.
2. **Declaration index** (`bc_index_build`) — open-addressing hash table,
   name → declaration. Built in two passes: explicit parser-supplied names
   first (authoritative, first-wins), then identifiers tokenized from each
   declaration's `text` — that second pass is what makes the every-name rule
   forgiving of under-reporting parsers. Lookups are **case-folded for PL/X,
   PLAS, HLASM, Pascal** (`bc_lang_folds_case`), case-sensitive for C/C++/
   Java. Keys live in an arena.
3. **Tiered closure** (`bc_closure`) — tier 0 = declarations directly
   referenced by the body (alphabetical for determinism); tier k+1 =
   declarations referenced from tier k's texts (default `--depth 1`).
   Character budget (default `--max-context 4000`); tier 0 is admitted
   before tier 1 so direct references are never crowded out; and if even the
   first declaration exceeds the budget it is sent anyway ("never send zero
   context when context exists").
4. **Snippet builder** (`bc_build_snippet`) — assembles the
   DOC/DECLARATIONS/CALLEES/FUNCTION layout byte-for-byte as the spec/skill
   define it. This layout **is the cache key input**, so its stability
   matters as much as its content.

### `graph.c` — parse, validate, render

- **Normalization before parsing:** `find_json_span` locates the first
  balanced top-level JSON object (string-aware), so a ```json fence or
  stray prose never costs a retry; anything non-whitespace outside the span
  marks the result *repaired* (a metric, see §6). `find_mermaid_block`
  handles the fallback: fenced ```mermaid or a bare `flowchart TD` region.
- **Two parse paths into one model:** JSON path via the vendored reader;
  Mermaid path via a small line parser (node shapes `[..]` `{..}` `(..)`
  map back to step/decision/call; a `[..]` node whose text starts with
  "Return" is re-kinded `return`; `%%` comments tolerated and stripped;
  `subgraph` rejected). Both paths feed the same `bg_validate`.
- **Validation** — exactly the spec's rules, including reachability via an
  explicit stack (bounded at 14 nodes, so no recursion, no allocation).
- **Mermaid serialization** (`bg_to_mermaid`) — code, not model output:
  quoted labels, `"` → `#quot;`, control characters dropped, edge labels
  stripped of `-`/`>`.
- **Canonical JSON** (`bg_canonical_json`) — what gets cached. The cache
  stores the validated **graph**, not the rendered Mermaid, so serializer
  improvements re-render old cache hits for free.
- **Call harvesting** (`bg_calls`) — collects `call`-node texts for the
  cross-reference feature at zero extra AI cost.

### `bob_client.c` — provider seam, cache, retry

- **Spawn** (`run_provider`) — `posix_spawn`, snippet on stdin, stdout
  captured, stderr → `/dev/null`. The argv comes from whitespace-splitting
  the command template with `{lang}` substituted; **no shell, ever**. Two
  concurrency subtleties that were actual bugs to avoid: a mutex serializes
  pipe-creation+spawn across worker threads, and all pipe fds get
  `FD_CLOEXEC` — without both, a concurrently spawned provider inherits a
  sibling's pipe ends, that sibling's stdin never sees EOF, and workers
  deadlock on each other until the timeout kills them. `SIGPIPE` is ignored
  process-wide so a child closing stdin early is a non-event.
- **Timeout** — a poll loop in 250 ms slices; `--ai-timeout` (default 60 s)
  of *silence* kills the child with SIGKILL. Note the known limitation in
  §7: this is an idle timeout, not a wall-clock deadline.
- **Cache** — key = `SHA-256(snippet ‖ skill-version ‖ command-template)`
  with NUL separators; entries are canonical graph JSON under
  `~/.cache/zdoc/<hex>.json` (`--ai-cache-dir` to move, `--ai-no-cache` to
  disable, `--ai-refresh` to bypass reads but still write). Writes go to a
  pid-suffixed temp file then `rename()` — atomic publish, so parallel
  workers and parallel *runs* can't corrupt entries; a corrupt entry is
  silently regenerated. Because the snippet embeds the body, the referenced
  declarations, and the callee briefs, **any change to any of those changes
  the key** — unchanged functions never hit Bob again, and an interrupted
  run resumes for free.
- **Retry policy** (`bc_generate`) — per symbol: call, normalize → parse →
  validate; on failure retry **once** with a contract-violation notice
  appended to the snippet; still failing → the caller records
  `diagram_error`. A failure never aborts the run. The last raw response is
  kept (truncated to 500 bytes) for the failure log.
- **Record mode** (`--ai-record <dir>`) — writes every request/response pair
  keyed by cache hash. This is how stub replay tests exist, and how the
  first real-provider run gets audited (§7).

### `main.c` — the filter

Parses flags; builds the command template (`--bob-cli`/`--bob-args` are
sugar that rewrites the default template; `--ai-command` replaces it);
parses the input JSON; per module builds the declaration index; per
executable symbol assembles closure + up to 8 callee lines
(`NAME: <signature> — <brief>`, matched against other extracted symbols,
case-folded where the language requires) and queues a job. A symbol with no
`body` (parser ran without `--ai-context`) is marked `diagram_error`
immediately.

Then the **worker pool**: `--ai-jobs N` threads (default 4, clamped 1–64)
pull jobs from a mutex-guarded counter — AI calls are I/O-bound, so this is
where wall-clock time comes from. Crucially, workers only *compute*;
**results are attached after `pthread_join`, in source order**, so output is
deterministic regardless of completion order (snapshot tests depend on it).

Finally: `call_edges` attachment (call-node texts tokenized and matched
against the module's symbol names, deduped, self-links skipped), the failure
log (`zdoc-ai-failures.log`: module, symbol, error, truncated raw response —
each entry is a candidate new skill example), the stderr stats line
`generated N, cached N, repaired N, failed N (provider calls: N)`, and an
exit code that is nonzero only when *everything* failed.

### `json.c` / `sha256.c` / `util.c`

Vendored minimal JSON DOM (reader/writer that preserves pass-through
fields), SHA-256, and small helpers (string builder, arena, file readers) —
the no-external-dependencies constraint made these part of the deliverable.

### `tests/`

- **Unit binaries** — `test_closure` (tokenizer, keyword filtering, index
  incl. every-name-from-text, case folding, tier/budget behavior, snippet
  layout) and `test_graph` (both parse paths, every validation rule,
  serialization escaping).
- **`run_tests.sh` — ten process-level sections** using stub providers in
  `tests/stubs/`: happy path (`bob-good`), repair (`bob-noisy` — fenced/
  prose-wrapped output), retry (`bob-flaky` — fails first call, succeeds
  second), mermaid fallback (`bob-garbage`), nonzero exit (`bob-exit1`),
  timeout (`bob-slow`), **cache byte-identity across two runs with one set
  of provider calls**, record mode, missing `--ai-context` body, and the
  end-to-end `zdoc-c-parser --ai-context | zdoc-bob-client` pipe.
- Stubs instead of a real CLI keeps CI fast, free, and deterministic — but
  it means the *skill* has never been graded by a real model (§7, item 3).

---

## 5. How to build and run it

```sh
cd ai/bob_client
make          # builds ./zdoc-bob-client
make test     # unit + stub process tests + e2e fixture snapshot

# real usage
zdoc-c-parser --ai-context src/*.c | zdoc-bob-client --bob-cli bob > augmented.json
```

Full flag set: `--input <file>`, `--bob-cli <path>`, `--bob-args "<args>"`,
`--ai-command "<tmpl>"`, `--ai-jobs N` (default 4), `--ai-timeout SECS`
(default 60), `--ai-cache-dir <dir>`, `--ai-no-cache`, `--ai-refresh`,
`--ai-record <dir>`, `--fail-log <path>`, `--max-context N` (default 4000),
`--depth N` (default 1), `--version`, `--help`.

---

## 6. The design decisions and their *why* (condensed)

| Decision | Why |
|---|---|
| Structured JSON graph, not raw Mermaid | Escaping/syntax is deterministic code; the model can't break or inject into rendered output. Mermaid accepted only as a validated fallback. |
| Content-addressed cache keyed on snippet ‖ skill-version ‖ command | Re-runs over unchanged source cost **zero** AI calls; interrupted runs resume free; skill or provider changes automatically invalidate. |
| Cache stores graph JSON, not Mermaid | Serializer improvements re-render old hits without regeneration. |
| Snippet on stdin, argv split without a shell | No injection surface, no argv size limits. |
| Worker pool + deferred in-order attachment | Concurrency where it pays (I/O-bound provider calls) with fully deterministic output. |
| Never abort; per-symbol retry once, then `diagram_error` | One bad symbol out of 500 must not kill a docs build; failures are logged with raw responses so they become skill examples. |
| `repaired` counter in the stats | The skill-quality metric: high repair rate = the output-contract instructions aren't landing = fix the prompt, not the parser or validator. |
| Closure lives in `bob_client`, once | The snippet **is** the cache key — five parsers reimplementing tiering/budget/format must agree byte-for-byte or caches silently die per language. The knobs are AI policy, not language facts; CALLEES needs a cross-symbol view only the filter has. Cost was a non-argument (closure is µs vs seconds of provider time). |
| Call-edge harvesting from `call` nodes | Cross-reference data for free — no extra AI call. |

---

## 7. What we might optimize / harden (the open list)

In priority order, per the discussion recorded in `NEXT-STEPS.md`:

1. **Wall-clock deadline in `run_provider`** *(the real hole)* — today
   `--ai-timeout` only counts *silence*: a provider trickling one byte per
   100 ms lives forever and grows the response buffer without bound. Add a
   hard cap (e.g. 10× the idle timeout, or `--ai-deadline`). Matters the
   moment a real streaming CLI replaces the stubs — a wedged model call is
   the normal failure mode, not an attack.
2. **`--bob-args` splitting is whitespace-only** — quoted arguments get
   mangled. Decision: *document* the limitation loudly rather than
   half-implement shell quoting; no shell is a security feature, and every
   real invocation so far fits whitespace splitting.
3. **First real-provider run in `--ai-record` mode** — the skill has never
   met a real model. Run a small real module, hand-review the pairs, commit
   a sanitized subset as replay fixtures, and watch the `repaired` counter
   to tune the skill text against reality — before a demo does it for us.
4. **Skill tuning loop** — `zdoc-ai-failures.log` entries are designed to
   become new `examples/` files; bump the skill version when the prompt
   changes (cache invalidation is automatic and intended).
5. **Parser-side `refs` field — spec'd, deliberately not built.** Optional
   per-symbol `"refs"` would let parsers exclude comment/string identifiers
   that the generic tokenizer can't. Consumer fallback (`bc_extract_refs`)
   already exists, so it's backward-compatible by construction. Trigger to
   revisit: a language whose comments the tokenizer genuinely can't
   approximate (HLASM column-based comments are the expected first case)
   *and* observed snippet pollution in real runs. Until both: premature
   optimization with five codebases as blast radius.
6. **Not worth doing** (litigated, recorded so it stays decided): moving
   closure into the parsers or a daemon (see §6), and macro pre-expansion
   for HLASM/PL/X remains a TODO — diagrams reflect unexpanded source.

The gate before merging any of it to `main`: `make test` green at the repo
root — 30 checks including the cache byte-identity section and the
parser→client end-to-end pipe, which exist precisely to catch the two ends
of the contract drifting apart during merges.
