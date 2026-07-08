# doc_extractor — comment block & tag extractor (shared)

Shared, language-agnostic stage that turns raw parser output into the common
documentation model consumed by the renderers. It parses doc-comment blocks and
their tags (brief, `@param`, `@returns`, notes) and resolves the module tree and
cross-references.

- **Layer:** `extractor/`
- **Planned binary:** `zdoc-doc-extractor`
- **Status:** Planned

## Input / output

- **Input:** the shared parser JSON (see [`../../parser/README.md`](../../parser/README.md)),
  read from stdin or file arguments. Accepts output from any parser.
- **Output:** normalised documentation-model JSON on stdout — the module tree plus,
  per symbol, `signature`, `brief`, `params`, `returns`, `notes` and `cross-references`.

Because every parser already emits the shared contract, `doc_extractor` is written
once and reused across all languages.

## Build & run

```sh
make                      # builds ./zdoc-doc-extractor
zdoc-c-parser file.c | ./zdoc-doc-extractor
make test
```

## Parallel parsing

Files are grouped by parser binary (Java, C/C++, PLX, ...) and each group is split
into chunks, batched to one parser invocation per chunk instead of one per file.
Chunks are dispatched to a thread pool (one thread per core) — pthreads on POSIX,
native Win32 threads on Windows — so both platforms parallelize the same way; there
is no sequential fallback. Chunk size is computed once from the **combined** file
count across every language group, sized to core count, so a repo with several
languages still fills the pool in about one round instead of each group spawning
its own core's worth of chunks and multiplying the total rounds needed.

See [`docs/ZDOC.md`](../../docs/ZDOC.md) for the full specification.
