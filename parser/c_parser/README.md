# ZDoc c_parser — C / C++ parser

The `parser/c_parser` component from the [ZDoc architecture](../../docs/ZDOC.md).
Written in C11 for speed: a single forward pass, no regex, no AST, no
per-token allocations.

## What it extracts

| Kind        | Source construct                                            |
|-------------|-------------------------------------------------------------|
| `function`  | Function/method definitions (incl. qualified `Foo::bar`, constructors, destructors, operators, templates) |
| `prototype` | Function/method declarations                                |
| `macro`     | `#define` — function-like always, object-like when documented |
| `type`      | `struct` / `class` / `union` / `enum` / `typedef` / `using` aliases |
| `variable`  | Documented file- or class-scope variables                   |

Doc comments (`/** */`, `/*! */`, `///`, `//!`) are parsed for
`@brief`, `@param` / `@tparam` (incl. `[in]`/`[out]`), `@return(s)` /
`@retval`, `@note` / `@remark(s)` / `@details`; unknown tags are kept
verbatim under notes. A block carrying `@file` / `@mainpage` becomes the
module-level doc instead of attaching to the next symbol.

## Why it is fast

- **Bodies are skipped, not parsed.** Most bytes of a source file sit inside
  `{ }` bodies; those are consumed by a brace-matching loop driven by a
  256-entry lookup table (strings, char literals, comments, raw strings and
  preprocessor lines are honoured so braces can't be miscounted).
- **One pass, no backtracking.** Declarations are recognized by a small
  state machine over spans of the input; nothing is tokenized into memory.
- **Lazy line numbers.** Newlines are counted with `memchr` only over the
  gap since the previous symbol, so hot loops never track lines.
- **Bump-arena strings.** All output strings live in an arena; teardown is
  a handful of `free`s.

## Performance

Measured on an Apple Silicon laptop (`-O3`, single thread):

- **~330 MB/s end-to-end** — 56 MB of `sqlite3.h` (100 concatenated copies)
  parsed *and* serialized to JSON (24,400 symbols) in 0.17 s.
- Memory stays flat: one padded copy of the source, one contiguous symbol
  array, one bump arena for strings. Teardown is a handful of `free`s.
- Validated against production headers: zlib.h, sqlite3.h, and libc++'s
  `__vector/vector.h` (179 symbols) parse cleanly with valid JSON.

## Fit for a flattened symbol tree (left-child/right-sibling)

A flattened binary tree — left-child/right-sibling links stored as `int32`
*indices* into the symbol array — is planned as the in-memory hierarchy for
the renderer (implemented separately). This parser was written so that
representation drops in without touching the scanner. Guarantees the
current implementation provides:

- **Strict pre-order emission.** `parse_decl_scope` emits a class's `type`
  symbol *before* recursing into its body (`parse_statement`, `'{'`
  handler, `kw_record` branch), so every parent precedes its children and
  children of one parent are contiguous. The links can therefore be
  stamped during emission (track the current parent's index in the `P`
  state, restore it when the recursion unwinds) or rebuilt afterwards in
  one linear post-pass over `cp_symbols()` — no re-parse needed.
- **One contiguous array, index-stable.** Symbols live in a single
  realloc-grown array (`cp_result.syms`); an index assigned at emit time
  stays valid even as the array grows, where pointers would dangle. Adding
  two `int32` fields to `cp_symbol` costs 8 bytes/node and zero extra
  allocations — heap-linked nodes would lose on both memory and traversal
  locality.
- **Source order is preserved.** Sibling order in the array is declaration
  order, so `next_sibling` chains come out already sorted for rendering.

Notes for the implementer — cases where nesting is *not* emitted, so the
parent index must simply be the enclosing scope's:

- `namespace` and `extern "C"` blocks recurse into `parse_decl_scope` but
  emit no symbol themselves (anonymous parent); their members attach to
  the surrounding scope.
- `enum` bodies and `typedef struct { ... } X;` bodies are fast-skipped
  (`skip_body`), so they never produce children.
- Anonymous `struct`/`class` (no tag name) recurses without emitting a
  parent symbol; members surface in the enclosing scope.
- Past the nesting guard (`st->depth >= 128`) record bodies are skipped,
  not recursed — leaf by construction.

`docs/ZDOC.md` only mandates the *output* module tree (file node → its
symbols); the flattened tree is the internal form the renderer will build
it from, and enables nested C++ rendering (`render` under `Widget`).

## Build & run

```sh
make            # builds ./zdoc-c-parser
make test       # runs it over tests/sample.c and tests/sample.cpp
./zdoc-c-parser src/foo.c include/foo.h > symbols.json
```

Output is a single JSON document, one `modules[]` entry per input file,
ready for the ZDoc extractor/renderer stages:

```json
{"zdoc_parser":"c","version":"0.1.0","modules":[
  {"file":"foo.c","language":"c",
   "module_doc":{"brief":"..."},
   "symbols":[
     {"kind":"prototype","line":26,"name":"widget_init",
      "signature":"int widget_init(void *anchor, unsigned flags)",
      "doc":{"brief":"...","params":[{"name":"anchor","desc":"..."}],
             "returns":"...","notes":"..."}}]}]}
```

## Library API

Link `c_parser.c` and include `c_parser.h`:

```c
cp_result *r = cp_parse_file("foo.cpp");
size_t n;
const cp_symbol *syms = cp_symbols(r, &n);
/* ... */
cp_result_free(r);
```

## Known limitations

- K&R-style parameter declarations are not recognized (the function is
  still found; the parameter decls end the signature early).
- Function-pointer *variables* at file scope are reported as prototypes.
- Heavily macro-obscured declarations (e.g. a macro that expands to the
  whole signature) are skipped or named after the macro, consistent with
  the "raw source, no pre-expansion" rule in docs/ZDOC.md.
