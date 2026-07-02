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

## Potential fix: flattened symbol tree (left-child/right-sibling)

Symbols are currently emitted as a **flat array in strict pre-order**:
`parse_decl_scope` emits a class's `type` symbol first, then recurses into
its body, so members always land immediately after their parent. The
nesting information itself is discarded, though — a method inside a class
and a free function are indistinguishable in the output.

If the renderer ever needs an explicit hierarchy (e.g. showing `render`
under `Widget` in the module tree), the right representation is a
**flattened binary tree**: left-child/right-sibling encoding stored as
`int32` *indices* into the existing array —

```c
typedef struct {
    /* ... existing fields ... */
    int32_t first_child;   /* -1 when leaf */
    int32_t next_sibling;  /* -1 when last */
} cp_symbol;
```

Why this form and not a pointer-linked tree:

- **8 bytes/node, zero extra allocations** — indices are stamped during the
  existing emit path (keep the current parent's index in the parser state,
  restore on recursion unwind; ~15 lines).
- **Cache-friendly traversal** — walking indices over a contiguous array
  beats chasing heap pointers, and indices survive `realloc` growth where
  pointers would dangle.
- **The parser is already fitted for it** — pre-order emission means every
  parent precedes its children, so the links can even be reconstructed in
  a single post-pass without touching the scanner.

A heap-allocated linked list of symbol nodes would be strictly worse on
both speed (pointer chasing, cache misses) and memory (2 pointers +
allocator overhead per node) than the current array — don't do that.

Note: `docs/ZDOC.md` only requires the *output* module tree (file node →
its symbols, one level deep); no in-memory representation is mandated, so
this is an optional enhancement for nested C++ rendering, not a spec gap.

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
