# parser/shared — shared global parser resources

Holds the process-wide state the language [parsers](../README.md) share while
running: the module-tree tables, the count of files to parse, the array the
parsers write their output into, and a counter of how many files are done.

- **Layer:** `parser/` → shared across every language parser
- **Builds on:** the [`module_tree`](../../extractor/doc_extractor/module_tree/README.md)
  tables (`modtree_dir_table_t` / `modtree_file_table_t`)
- **Status:** In progress — the shared output structs (`Module` / `Symbol` /
  `InputParam`) are defined and `plx_parser` is wired over to them; `c_parser`
  and `java_parser` are not yet migrated (see below)

---

## What's here

| Global                                    | Type                     | Purpose                                                    |
|-------------------------------------------|--------------------------|------------------------------------------------------------|
| `global_dir_table`                        | `modtree_dir_table_t`    | Directory table for the sources being documented           |
| `global_file_table`                       | `modtree_file_table_t`   | File table — the set of files the parsers walk             |
| `files_count`                             | `int`                    | Number of files to parse, taken from the file table        |
| `global_parsed_files_arry`                | `Module*`                | Output array, one entry per file (allocated by init)       |
| `finished_files`                          | `_Atomic int`            | How many files have been parsed; atomic for concurrent use |

`init_resources()` reads the file count from the module-tree interface
(`number_of_files`), stores it in `files_count`, and allocates
`global_parsed_files_arry` with room for one entry per file. It returns `0` on
success and `-1` on failure (bad count or allocation failure).

---

## Shared output structs

These are the unified output format every language parser is meant to emit,
factored out of the per-language `plx_parser.h` / `c_parser.h` / `java_parser.h`
structs. Field names are provisional — each parser will be adapted to them.

```c
typedef struct {
    char *name;
    char *description;
} InputParam;

typedef struct {
    char *name;          /* symbol name (function/method/routine/...) */
    char *description;   /* brief summary  (c/java `brief`)           */
    char *signature;     /* comment-free declaration signature        */
    InputParam *input;   /* parameter list                            */
    int inputCount;      /* number of parameters                      */
    char *output;        /* return-value / OUTPUT description          */
    char *notes;         /* @throws / caveats / misc remarks          */
    uint32_t line;       /* 1-based line of the declaration            */
    char *type;          /* symbol category (c `kind`); NULL if unused */
    char *diagram;       /* AI-mode diagram; NULL until online mode    */
} Symbol;

typedef struct {
    char *filename;
    Symbol *symbols;
    int symbolCount;
    int symbolCap;       /* geometric-growth capacity of symbols[]    */
} Module;
```

**Mapping from the per-language structs**

| Shared field         | plx           | c_parser            | java_parser  |
|----------------------|---------------|---------------------|--------------|
| `Symbol.name`        | `name`        | `name`              | `name`       |
| `Symbol.description` | `description` | `doc.brief`         | `brief`      |
| `Symbol.signature`   | `signature`   | `signature`         | `signature`  |
| `Symbol.input`       | `input`       | `doc.params`        | `params`     |
| `Symbol.output`      | `output`      | `doc.returns`       | `returns`    |
| `Symbol.notes`       | —             | `doc.notes`         | `notes`      |
| `Symbol.line`        | `line`        | `line`              | `line`       |
| `Symbol.type`        | —             | `kind`              | —            |
| `Symbol.diagram`     | —             | —                   | `diagram`    |
| `InputParam.description` | `description` | `desc`          | `description`|

Fields a parser has no logic for yet are left `NULL`/`0` (the symbol is
`memset` to zero on allocation).

**Fields not yet represented** (present in a source parser, no shared home — revisit
before wiring those parsers over):

- c_parser's error string (`cp_error`) has no `Module`-level home.

**Integration status:** `plx_parser` and `java_parser` are wired over to these
structs. `plx` populates `line`/`type` (always `"procedure"`) and leaves
`notes`/`diagram` `NULL`; `java` populates `line` and every doc field
(`description`/`output`/`notes`/`input`/`diagram`) and leaves `type` `NULL`.
`c_parser` is not yet migrated.

## Files

| File                                                     | Purpose                                                     |
|----------------------------------------------------------|-------------------------------------------------------------|
| [`parser_shared.h`](parser_shared.h)                     | Declares the shared globals, the output structs (`Module`/`Symbol`/`InputParam`), and `init_resources` |
| [`parser_shared.c`](parser_shared.c)                     | Defines the globals and implements `init_resources`         |
| [`global_resources_test.c`](global_resources_test.c)     | Self-check: mocks a one-file table, runs init, asserts state |

---

## Building & running the test

`global_resources_test.c` builds a mock file table with a single file, runs
`init_resources()`, then asserts every global lands in the expected state.
Compile it together with the shared implementation and the module-tree tables:

```sh
cd parser/shared
cc -Wall -Wextra -std=c11 \
   global_resources_test.c parser_shared.c \
   ../../extractor/doc_extractor/module_tree/modtree_table.c \
   -o global_resources_test
./global_resources_test
```

Expected output:

```
All global resource checks passed.
```

See [`docs/ZDOC.md`](../../docs/ZDOC.md) for the full specification.
