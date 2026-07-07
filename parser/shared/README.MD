# parser/shared — shared global parser resources

Holds the process-wide state the language [parsers](../README.md) share while
running: the module-tree tables, the count of files to parse, the array the
parsers write their output into, and a counter of how many files are done.

- **Layer:** `parser/` → shared across every language parser
- **Builds on:** the [`module_tree`](../../extractor/doc_extractor/module_tree/README.md)
  tables (`modtree_dir_table_t` / `modtree_file_table_t`)
- **Status:** In progress — the output struct is still a placeholder (see below)

---

## What's here

| Global                                    | Type                     | Purpose                                                    |
|-------------------------------------------|--------------------------|------------------------------------------------------------|
| `global_dir_table`                        | `modtree_dir_table_t`    | Directory table for the sources being documented           |
| `global_file_table`                       | `modtree_file_table_t`   | File table — the set of files the parsers walk             |
| `files_count`                             | `int`                    | Number of files to parse, taken from the file table        |
| `global_parsed_files_arry`                | `TempName*`              | Output array, one entry per file (allocated by init)       |
| `finished_files`                          | `_Atomic int`            | How many files have been parsed; atomic for concurrent use |

`init_resources()` reads the file count from the module-tree interface
(`number_of_files`), stores it in `files_count`, and allocates
`global_parsed_files_arry` with room for one entry per file. It returns `0` on
success and `-1` on failure (bad count or allocation failure).

---

## ⚠️ `TempName` is a temporary placeholder — change it

```c
typedef struct {
    int testVar;
} TempName;
```

`TempName` is **not** the real parser output format. It is a stand-in: the name
is a placeholder and its single `testVar` field is garbage that only exists so
`init_resources()` has something to `sizeof` and allocate for the test.

It should be replaced with the real, agreed-on global output format the parsers
emit — the shape described in the [parser JSON contract](../README.md#shared-json-contract).
When it changes:

- Rename `TempName` to the real type name and give it the actual fields.
- Update the `global_parsed_files_arry` declaration in
  [`parser_shared.h`](parser_shared.h) and its definition in
  [`parser_shared.c`](parser_shared.c).
- Update [`global_resources_test.c`](global_resources_test.c) to match.

---

## Files

| File                                                     | Purpose                                                     |
|----------------------------------------------------------|-------------------------------------------------------------|
| [`parser_shared.h`](parser_shared.h)                     | Declares the shared globals, `TempName`, and `init_resources` |
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
