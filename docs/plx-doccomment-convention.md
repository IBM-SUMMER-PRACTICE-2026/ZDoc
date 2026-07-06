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
| `Title`, `TITLE`, `Routine`, `name` | `name` | Human-readable name/title of the procedure |
| `Logic`, `Function`, `FUNCTION`, `function` | `description` | What the procedure does |
| `Input`, `INPUT`, `input` | `input` | Parameters accepted |
| `Output`, `OUTPUT`, `output` | `output` | Return value / return codes |

The lowercase `name` / `function` / `input` / `output` spellings appear in the
[Method Prolog blocks](#method-prolog-blocks-plxmac-macro-routines) used by
`.plxmac` macro routines — they normalize to the same fields (case-insensitive
matching already covers them).

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
  - an `?AsaXMac ProcEntry(NAME)` macro marker wrapping the procedure — see
    [Method Prolog Blocks](#method-prolog-blocks-plxmac-macro-routines) below.

Everything from block start to block end belongs to one procedure's doc comment.

The procedure's own name (from the `PROC` statement or entry marker) should be
**cross-checked** against the `name`/`Title`/`Routine` field to confirm correct
association — don't rely on proximity alone, since module-level header blocks
(e.g. `MODULE-NAME =`, `FUNCTION =` in the file's top specification section) can
look similar but aren't tied to any single procedure.

## Method Prolog Blocks (`.plxmac` macro routines)

`.plxmac` macro-library files (see [`student_grades.plxmac`](student_grades.plxmac))
do **not** use the plain `NAME: PROC(...)` procedure form. Their routines are
defined by the `?AsaXMac` macro processor, and each is preceded by a **Method
Prolog** comment box instead of the per-line `/* Label: ... @TAG*/` format
described above. A parser targeting `.plxmac` must recognize this alternate
convention; the extracted fields normalize to the same `name` / `description` /
`input` / `output` symbol as everywhere else.

> **Status:** this section is derived from the **single available example**
> (`student_grades.plxmac`). It documents what that file demonstrates, but the
> `?AsaXMac` macro form should be validated against further real PL/X macro
> sources before edge cases are treated as settled — do not assume the shapes
> below are exhaustive.

### Block Delimiters

The prolog is a **single multi-line comment** (not one comment per line). Its
extent is marked by two banner lines:

```
*/** Start of Method Prolog *******************************************
 ...
**** End of Method Prolog ********************************************/
```

- **Start:** a line whose text is `Start of Method Prolog` surrounded by `*`
  padding. Note the leading `*/**`: the PL/X comment opens at the `/*` inside
  this banner; the leading `*` is a listing-border artifact that sits, strictly
  read, outside the comment. Recognize the block by the **`Start of Method
  Prolog` banner text**, not by the exact delimiter bytes.
- **End:** a line whose text is `End of Method Prolog` padded with `*`, closed
  by the `*/` that terminates the whole comment.
- **Left border:** every interior line begins with a single `*` box character
  (at column 0), followed by whitespace. Strip this leading `*` and surrounding
  whitespace before parsing the line's content.

### Field Lines

Interior lines use the same `label: content` shape as the main convention, with
lowercase labels and no `@TAG` suffix (these blocks are not per-line tagged):

```
*  name:    GetLetterGrade:
*
*  function: Assign a grade letter based on the student score
*
*  input:   score         - score of a student
*                           Fixed(31)
*           letter        - grade letter to be set based on score
*                           Char
*
*  output:  assigns new value to letter
```

- `name`, `function`, `input`, `output` normalize to `name`, `description`,
  `input`, `output` respectively (case-insensitive, per the label table above).
- A trailing `:` on the `name` value (`GetLetterGrade:`) is decoration — strip it.
- Blank border lines (`*` alone) are **padding** — skip them, don't treat them
  as field terminators or continuations.
- The `input` field uses the [`name - description` row format](#2-name---description-rows);
  a row's type can wrap onto the next line with no ` - ` separator (e.g.
  `Fixed(31)` / `Char` above) and joins to the preceding row's description.

### Procedure Entry / Exit Markers

Immediately after the prolog, and again after the routine body, two additional
markers appear:

```
 /* ++H "GetLetterGrade": entry to assign a letter grade         @L0A*/
 ?AsaXMac ProcEntry(GetLetterGrade)
          EDTL(score    := Fixed(31) ByValue
              ,letter   := Char ByAddr)
            ;
 ...
 ?AsaXMac ProcEnd(GetLetterGrade);
 /* ++H End "GetLetterGrade"                                     @L0A*/
```

- **`?AsaXMac ProcEntry(NAME)`** is the routine's signature statement — the
  `.plxmac` equivalent of `NAME: PROC(...)`. It is the **block-end signature**
  that closes the doc block, and `NAME` is the authoritative procedure name to
  cross-check against the prolog's `name` field.
- **`EDTL(param := Type ByValue|ByAddr, ...)`** declares the parameter list, one
  parameter per `name := Type passing-convention` entry, comma-separated (the
  comma often leads the continuation line). An optional **`Returns(...)`** clause
  follows `EDTL` before the terminating `;` — in the example this is always
  `Returns(IsA(<type>))`.
- The signature can span multiple lines up to the terminating `;`, exactly like
  a plain `PROC` signature — scan to the first `;` at paren depth 0, ignoring
  `;`/parens inside comments and strings.
- **`?AsaXMac ProcEnd(NAME)`** marks the routine's end (it may carry a trailing
  `! ReturnProcess(Return(...))` clause). This is *not* part of the doc block.
- **`/* ++H "NAME": ... */`** and **`/* ++H End "NAME" */`** are secondary
  index/entry markers (also seen as `/*++h '<NAME>': ENTRY */` in `.plx`). They
  are metadata banners, not doc-comment fields — do not fold them into any field.

### Association Rule

Because the prolog box, the `++H` markers, and the `ProcEntry` statement all
carry the routine name, cross-check them: the `name` field from the prolog, the
name in `?AsaXMac ProcEntry(NAME)`, and the `++H "NAME"` marker should agree.
By analogy to the `PROC`-name vs `Title`-field check in the plain convention, a
mismatch is **recommended** to be reported as a warning (this rule is inferred,
not observed in the example — the sample file's names are consistent).
