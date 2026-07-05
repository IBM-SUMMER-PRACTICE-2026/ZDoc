# PL/X Procedure Doc-Comment Convention

> Reference spec for the PL/X doc-comment parser in ZDoc. Describes the comment
> block format that precedes each procedure so the parser can extract structured
> documentation (name, description, inputs, outputs).

## Overview

Each procedure in this codebase is preceded by a structured comment block that
documents its purpose, inputs, and outputs. The block uses a `label: content`
pattern, one label per logical field, with optional multi-line continuations. A
trailing change-activity tag (e.g. `@L0A`) appears on most lines and is **not**
part of the documented content.

## Line Format

```
/*  <LABEL><padding>: <content><padding>          @TAG*/
```

| Part | Description |
|------|-------------|
| `/* */` | Standard PL/X comment delimiters. One field spans one or more consecutive comment lines. |
| `<LABEL>` | A field name. Case is inconsistent across the codebase (`Title`, `TITLE`, `Routine`) — treat as **case-insensitive**. |
| padding before `:` | Variable-width whitespace, sometimes present before the colon (`Routine :`) for column alignment. **Ignore** when parsing. |
| `:` | Separates label from content. Always present on a label-opening line. |
| `<content>` | Free text for that field. May be empty (field opened but filled on the next line). |
| `@TAG` | A change-activity marker (`@L0A`, `@L1A`, `@00C`, `@L2A`, ...). Always at the end of the line, right before `*/`. **Strip this** — it is metadata about when the line was added, not documentation content. |

## Recognized Labels

| Label variants seen | Normalized field | Meaning |
|---------------------|------------------|---------|
| `Title`, `TITLE`, `Routine` | `name` | Human-readable name/title of the procedure |
| `Logic`, `Function`, `FUNCTION` | `description` | What the procedure does |
| `Input`, `INPUT` | `input` | Parameters accepted |
| `Output`, `OUTPUT` | `output` | Return value / return codes |

Treat this list as **non-exhaustive** — new files may introduce label spellings
not seen yet (e.g. plurals, abbreviations). The parser should normalize via a
**lookup table**, not a hardcoded switch, so new synonyms can be added without
changing parse logic.

## Continuation Lines

A field's content can wrap across multiple comment lines. A continuation line
has **no label + colon** — it's just indented content, still inside `/* ... */`,
still possibly tagged with `@TAG`:

```
/* Input   : Student ID - Fixed(31)                             @L0A*/
/*           Array of 5 grades - Fixed(31)                      @L0A*/
```

Both lines belong to the `input` field. Concatenate their content (trimmed) with
a space or newline, in order.

## Input Field Formats

The content of the `input` field itself follows one of three shapes. The parser
should detect them in this order:

### 1. Name list with `Where <name> is:` description blocks

The label line carries a **comma-separated list of parameter names**. Each
parameter's description follows in its own `Where <name> is:` block, with the
description items enumerated `1)`, `2)`, ...:

```
/*  Input: ID, Name, Year                                       @L0A*/
/*                                                              @L0A*/
/*         Where ID is:                                         @L0A*/
/*                                                              @L0A*/
/*           1) Fixed(31) number                                @L0A*/
/*                                                              @L0A*/
/*         Where Name is:                                       @L0A*/
/*                                                              @L0A*/
/*           1) A character array with maximum length of 30     @L0A*/
```

Rules:

- The presence of any `Where <name> is:` line selects this format.
- `Where` and the parameter names are matched **case-insensitively** against
  the declared name list.
- The enumeration markers (`1)`, `2)`, ...) are presentation only — **strip**
  them. Multiple items belonging to one parameter are joined with `"; "`;
  a wrapped item continues on the next line and is joined with a space.
- A `Where` block naming a parameter that is not in the declared list should be
  reported as a **warning**, but its content kept (added as an extra parameter).
- A declared name with no `Where` block is a parameter with an empty
  description.

### 2. `name - description` rows

Each line is one parameter; name and description are separated by ` - `:

```
/* Input   : Student ID - Fixed(31)                             @L0A*/
/*           Array of 5 grades - Fixed(31)                      @L0A*/
```

This format applies only when **every** content line contains the ` - `
separator.

### 3. `None` / free text

`Input: None` (case-insensitive) means the procedure takes **no parameters**.
Any other content that matches neither shape above is kept verbatim as a single
unstructured entry.

## Blank / Padding-Only Lines

A line may be pure padding with no content and no label:

```
/*                                                              @L0A*/
```

This should be **skipped**, not treated as a continuation that resets or clears
the current field.

## Block Boundaries

- **Start:** the first labeled comment line after a section divider (commonly
  `@EJECT;` or a banner line of `/***...***/`) preceding a procedure.
- **End:** the line immediately before the procedure's signature statement —
  either:
  - a plain `NAME: PROC(...)` statement, or
  - (if applicable) an `?AsaXMac ProcEntry` / structured marker wrapping the
    procedure — **confirm this format before relying on it.**

Everything from block start to block end belongs to one procedure's doc comment.

The procedure's own name (from the `PROC` statement or entry marker) should be
**cross-checked** against the `name`/`Title`/`Routine` field to confirm correct
association — don't rely on proximity alone, since module-level header blocks
(e.g. `MODULE-NAME =`, `FUNCTION =` in the file's top specification section) can
look similar but aren't tied to any single procedure.
