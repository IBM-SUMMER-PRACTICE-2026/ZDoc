# ai/ — AI Assisted mode

A small local developer tool. Bob is run **per file** by a developer, not as a
codebase-wide service.

## `zdoc_ai`

```sh
make -C ai zdoc_ai
./ai/zdoc_ai path/to/File.c        # or .java / .plx
```

`zdoc_ai` parses one source file with the real parser to find each function and
its **starting line**, then prints:

1. the Bob prompt (the diagram contract), and
2. the file to read plus the list of functions, each with its starting line.

Bob reads the file itself and returns one Mermaid `flowchart` per function,
each headed by `## line <N>: <name>`. The **starting line is the key** that ties
every returned diagram back to the symbol it belongs to.

That's the whole thing — no Bob subprocess, no snippet assembly. See
[`docs/ZDOC.md`](../docs/ZDOC.md) for the wider spec.
