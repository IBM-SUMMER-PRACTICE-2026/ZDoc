# extractor/ — shared module tree

- [`doc_extractor/module_tree`](doc_extractor/module_tree/) — walks the source tree
  and builds the shared directory/file tables (`modtree_dir_table_t`/
  `modtree_file_table_t`) that the daemon and both [renderers](../renderer/) read
  directly.

There used to be a `doc_extractor` stage here that copied the daemon's parsed
`Module` array plus this same tree into its own model before handing it to the
renderers. That copy added no real value once every parser already emits the same
shared `Module`/`Symbol` shape (see [`parser/shared`](../parser/shared/)), so it was
removed - each renderer now reads the module_tree tables and the parsed `Module`
array directly instead.

See [`docs/ZDOC.md`](../docs/ZDOC.md) for the full specification.
