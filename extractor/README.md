# extractor/ — shared documentation extraction

Language-agnostic stage that sits between the [parsers](../parser/) and the
[renderers](../renderer/).

- [`doc_extractor`](doc_extractor/) — normalises parser JSON into a common
  documentation model: comment blocks, tags (`@param`, `@returns`, …), briefs and
  cross-references, ready for rendering.

See [`docs/ZDOC.md`](../docs/ZDOC.md) for the full specification.
