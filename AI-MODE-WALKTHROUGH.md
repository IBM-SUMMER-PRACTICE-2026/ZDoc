# The ZDoc AI Mode Walkthrough — every choice, justified

> A guided tour of everything built on `feature/ai-skill-context`, written
> the way I'd teach it. (You asked for the best CSS teacher ever — you're
> getting the best C teacher ever; there isn't a single stylesheet in this
> repo and I refuse to apologize for it.)

---

## Lesson 0 — The one-sentence summary

When ZDoc runs with `--mode ai`, each extracted function is sent — with
*just enough* context — to the Bob CLI, which returns a **JSON graph**;
ZDoc validates that graph mechanically, renders the Mermaid itself, caches
the result forever, and never lets one bad response ruin the run.

```
source ──> zdoc-c-parser --ai-context ──> zdoc-bob-client ──> renderer
                (bodies + declarations)      (diagrams in,
                                              JSON out)
```

Everything below exists to make that sentence true. Let's go component by
component, and for each one I'll tell you *what* I did and *why I'd defend
it in a code review*.

---

## Lesson 1 — Docs before code (`docs/zdoc-ai-mode.md`)

**What:** the first commit-worthy artifact was a spec, not code.

**Why:** `AGENTS.md` declares `/docs` the source of truth for this repo.
If I'd written code first and docs later, the docs would describe the code.
Written this way, the code has to *answer to* the docs — and reviewers can
check my implementation against a contract I couldn't quietly bend. The
spec also explicitly supersedes the original brief, so nobody ever has to
guess which of two conflicting documents wins.

---

## Lesson 2 — The skill returns JSON, not Mermaid (`.bob/skills/zdoc-diagram/`)

**What:** the original brief asked the model for a fenced Mermaid block and
then policed it with rules like "no unescaped brackets in node text."
I changed the contract: the model returns

```json
{"nodes":[{"id":"A","kind":"step","text":"Entry: INITPROC"}],
 "edges":[{"from":"A","to":"B","label":"No"}]}
```

and **ZDoc serializes the Mermaid deterministically**.

**Why — and this is the core design decision of the whole feature:**
every "output contract" rule in the brief existed because the *model* was
writing *syntax*. Syntax generation is exactly what deterministic code is
good at and language models are bad at. Move the syntax to our side and:

- escaping becomes impossible to get wrong (it's our serializer's job —
  a `"` in node text becomes `#quot;`, end of story);
- validation becomes a schema check plus graph sanity (unique ids,
  reachability, labeled decision edges) instead of bracket-counting;
- the same validated graph can later render to Graphviz or ASCII for free;
- the retry prompt gets simpler, so retries succeed more often.

We still accept a bare `flowchart TD` block as a fallback — a stubborn
model response is money already spent; parse it rather than waste it.

**The golden examples** (PL/X, C, HLASM) survive from the brief because
they're genuinely the best part of it: they *define granularity by
demonstration*. I added a **negative example** (the same C function
diagrammed wrongly, line-by-line) because models — like students — learn
faster from a marked-up wrong answer than from three perfect ones.

---

## Lesson 3 — Closure assembly (`ai/bob_client/closure.c`)

**What:** for each function we send only the declarations its body actually
references: tokenize the body, look each identifier up in a per-module
hash index, take the hits (tier 0), then what *those* declarations
reference (tier 1), under a 4000-character budget.

**Why each piece:**

- **Tokenizer over parser.** The identifier pattern
  `[A-Za-z_$#@][A-Za-z0-9_$#@]*` deliberately over-collects (`$#@` is for
  HLASM/PL/X names). Precision comes from the *index lookup* — an
  identifier that resolves to nothing costs nothing. Building seven
  real per-language parsers for this would be months of work for zero
  extra precision.
- **The every-name rule.** A PL/X `DCL 1 CB BASED(CBPTR)` block must be
  findable via `CB`, every member, *and* `CBPTR` — bodies reference
  members and base pointers, never the structure name. The index gets
  explicit parser-provided names first (authoritative), then identifiers
  tokenized from each declaration's own text (fills the gaps when a
  parser under-reports). Explicit names win collisions — first-wins,
  never displaced by a tokenized guess.
- **Case folding per language.** PL/X, PLAS, HLASM, Pascal fold to
  uppercase; C/C++/Java stay exact. One `bc_lang_folds_case()` predicate,
  consulted at insert and lookup — not scattered `if`s.
- **Tier 0 before tier 1, always.** A transitive declaration must never
  crowd out one the function references directly — so tier 0 is admitted
  to the budget completely before tier 1 gets a byte. And if even the
  first declaration exceeds the budget, it goes anyway: *never send zero
  context when context exists.*
- **Sorted iteration everywhere.** Alphabetical refs, source-ordered
  output. Snapshot tests die without determinism; so does cache hygiene.
- **Two additions the brief didn't have:** the symbol's own doc comment
  (`DOC:` section — the best human-written hint for naming steps, and the
  brief inexplicably omitted it) and one-line callee context
  (`CALLEES: TERMPROC: <signature> — <brief>`) so the model writes
  "Call TERMPROC to release chain" instead of "call function".

---

## Lesson 4 — The graph core (`ai/bob_client/graph.c`)

**What:** parse the model's JSON (or fallback Mermaid), validate, serialize.

**Why the validation rules are what they are:**

- **≤ 14 nodes** — the skill says 5–12 is the target; 14 is the hard stop.
  A 40-node diagram isn't documentation, it's a crime scene photo.
- **Reachability from the first node** — an unreachable node is always a
  hallucination or a broken edge list; either way the diagram lies.
- **Labeled decision out-edges** — an unlabeled branch is unreadable, and
  it's the single most common model sloppiness. Cheap to check, high value.
- **Repair before retry** — normalization strips fences, surrounding
  prose, `%%` comments *first*. Most contract violations are wrapper
  noise ("Sure! Here's your diagram: …"). Repairing locally is free;
  a retry is a paid model call. Only after repair fails do we spend money.

**Why the fallback Mermaid parser exists at all:** the JSON contract is an
instruction to a model, not a law of physics. If Bob ships a mode that
insists on Mermaid, ZDoc keeps working. The fallback parses into the *same
graph model*, so everything downstream (validation, serialization,
call harvesting) is shared — one pipeline, two front doors.

---

## Lesson 5 — The provider client (`ai/bob_client/bob_client.c`)

**What:** spawn the CLI, feed the snippet on stdin, enforce a timeout,
retry once, cache, record.

**Why each choice:**

- **`posix_spawn`, snippet on stdin, never a shell.** argv has size
  limits; shell-quoting a source snippet into a command line is an
  injection bug factory. stdin has neither problem. The command template
  (`bob explain --diagram --brief --lang {lang}`) is split on whitespace
  by us — no `system()`, ever.
- **The cache key is `SHA-256(snippet ‖ skill-version ‖ command)`.**
  Change the function → new snippet → new key. Change the skill → new
  version → every diagram regenerates (correct: the contract changed).
  Change the model/CLI → new command → same. Anything that *should*
  invalidate the cache is *in* the key; nothing else is.
- **The cache stores the graph JSON, not the rendered Mermaid.** If we
  improve the serializer next month, every cache hit re-renders with the
  improvement. Caching the Mermaid would freeze old bugs into the docs.
- **Write via temp file + `rename()`** — atomic publish; a crashed run
  can't leave a half-written cache entry to poison the next one.
- **Retry exactly once, then move on.** Two failures on the same snippet
  means the problem is the snippet or the model, not luck. The failure
  (module, symbol, truncated raw response) goes to
  `zdoc-ai-failures.log` so bad cases can become new skill examples —
  the failure log is a training-data pipeline in disguise.

**And the war story — read this part twice.** The first full test run
failed intermittently: 2 of 3 symbols "timed out" under concurrency while
one always succeeded. Root cause: threads spawning providers
concurrently — each child inherited copies of its *siblings'* pipe file
descriptors (pipes default to inheritable). A child's `cat` never saw
stdin EOF because a sibling still held a copy of the write end; the
siblings deadlocked on *each other* until the 60-second timeout killed
them. The first-spawned child had no siblings yet — which is exactly why
one symbol always survived. Fix: `FD_CLOEXEC` on every pipe fd, plus a
mutex around pipe-creation+spawn to close the race window between the two
calls. **This is why the tests exist.** No amount of staring at the code
finds that bug; three shell-script stubs and a counter file found it in
one run.

---

## Lesson 6 — The filter binary (`ai/bob_client/main.c`)

**What:** `zdoc-bob-client` reads the pipeline JSON, augments it, writes it
back out. Worker pool inside; the JSON never fragments across processes.

**Why:**

- **A standalone JSON filter, not in-process wiring.** The brief wanted a
  per-symbol loop inside a monolith; the repo's architecture (per the
  scaffold that landed on `main`) is standalone binaries piping JSON.
  I followed the repo — `AGENTS.md` makes it authoritative — and it's the
  better design anyway: offline mode can't regress because offline mode
  *never executes this binary*.
- **Worker pool with an index-based queue.** Provider calls are I/O-bound;
  4 concurrent calls (configurable `--ai-jobs`) turn hours into minutes.
  Results attach to symbols in *source order* after all threads join, so
  output is deterministic regardless of completion order.
- **Vendored JSON + SHA-256** (~600 lines total). The constraint was "no
  new external dependencies," and for a mainframe-adjacent internal tool
  that's the right constraint: every dependency is a procurement
  conversation. Both are single-file, boring, and tested through the
  suites that use them.
- **Never abort.** A symbol without `--ai-context` data, a garbage
  response, a dead provider — each marks *that symbol* (`diagram_error`)
  and the run continues. Documentation generation is a batch job; batch
  jobs that die at item 947 of 1000 make people turn features off.
- **Call-edge harvesting.** The skill makes the model mark calls as
  `call` nodes with real names. We tokenize those texts and match them
  against the module's extracted symbols → `call_edges` per symbol. That
  feeds the spec's Cross-references section *and* is the seed data for
  the roadmap's call graph — information we already paid for; refusing
  to collect it would be the waste.

---

## Lesson 7 — The parser's `--ai-context` flag (`parser/c_parser`)

**What:** with the flag, functions carry `body` + `line_end`, and each
module carries `declarations` (names + verbatim text). Without the flag —
**byte-identical output**, verified by diffing against the pre-change
binary.

**Why the details are the way they are:**

- **Flag-gated, not always-on.** Bodies can dominate output size, and the
  hard constraint was "offline mode bit-identical." A flag the offline
  path never passes is the strongest possible guarantee.
- **`body` includes the signature,** because the skill's golden examples
  show full functions, and the model names the entry node from the
  signature line.
- **Struct members don't emit fragment declarations.** A member like
  `int refcount;` inside a struct must resolve to the *whole struct's*
  text (that's the every-name rule's point). So declaration capture is
  suppressed inside record bodies (`record_depth`), and members ride
  inside their record's verbatim text — which the closure indexer
  tokenizes anyway. One `int` counter, correct semantics.
- **Every `#define` is a declaration** (capped at 500 chars) — replacement
  lists are exactly what gives `RC_OK` its meaning in a diagram label.

---

## Lesson 8 — The tests (`ai/bob_client/tests/`)

**What:** two unit suites (closure, graph — table-driven), six stub
providers (`bob-good`, `bob-noisy`, `bob-flaky`, `bob-garbage`,
`bob-exit1`, `bob-slow`), a process-level shell suite, and a true
end-to-end run of `zdoc-c-parser --ai-context | zdoc-bob-client`.
30 checks, all green, one command: `make test` at the repo root.

**Why this shape:**

- **The stubs are the spec of provider behavior.** Each one is a
  five-line shell script impersonating one failure mode. Together they
  cover: clean output, prose-wrapped output (repair, *not* retry —
  asserted by counting invocations in a file), fail-then-succeed (retry),
  always-fail (fallback + failure log + raw capture), nonzero exit,
  and a 30-second sleep against a 1-second timeout (the kill path,
  asserted by wall-clock).
- **Invocation counting via `ZDOC_STUB_COUNT`** is the load-bearing
  trick: "repaired, not retried" and "cached, not re-called" are claims
  about *how many processes ran*, and the counter file is the only
  honest way to assert that.
- **The cache test runs the same input twice** and asserts three things:
  zero provider calls the second time, `cached 3` in the stats, and
  byte-identical output. That last one is the determinism guarantee
  earning its keep.
- **The closure fixture mirrors the PL/X golden example** — the same
  `CB`/`CBPTR`/`ANCH` block the skill teaches with — so the tests and
  the skill can never drift apart silently.
- And the payoff, once more: **this suite caught a real concurrency
  deadlock on its first full run.** Tests that only confirm the happy
  path are decoration; these earned their lines.

---

## Lesson 9 — Known limitations (the honest section)

Where I'd attack this system, in order:

1. **A provider that streams output forever** — the timeout only advances
   while the pipe is *idle*, so a provider trickling bytes indefinitely
   defeats the wall clock and grows the response buffer without bound.
   Real Bob won't do this; a malicious "provider" could.
2. **`--bob-args` has no shell-quoting** — arguments with embedded spaces
   split wrongly. Documented behavior, but a sharp edge.
3. **Call-edge matching is token-based** — a call node reading
   "Call the init routine" where `init` happens to be a symbol name will
   cross-link falsely. Harmless (links, not code), but imprecise.
4. **Macro-heavy HLASM/PL/X** still diagrams macro *invocations*, not
   logic — pre-expansion remains a TODO hook, per the spec.
5. **K&R function definitions** in the C parser end the signature early
   (inherited limitation, documented in the parser README).

---

## Closing

The theme, if you take one thing away: **move work from the probabilistic
component to the deterministic one, every time you can.** The model
decides *what the steps are*; code decides how they're spelled, escaped,
validated, cached, retried, and parallelized. That division is why the
whole thing is testable with six shell scripts — and why the tests found
a kernel-level fd bug instead of a prompt tweak.

Class dismissed.
