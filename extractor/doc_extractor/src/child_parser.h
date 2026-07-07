/*
 * Everything to do with running a language parser as a child process on one
 * file and interpreting its JSON output (parser/README.md's contract) -
 * separated from doc_extractor.c's own orchestration (walking the tree,
 * assembling the combined model).
 */
#ifndef ZDOC_DOC_EXTRACTOR_CHILD_PARSER_H
#define ZDOC_DOC_EXTRACTOR_CHILD_PARSER_H

#include "doc_extractor.h"

typedef struct {
    const char *ext;
    const char *language;
    const char *parser_bin; /* zdoc-<lang>-parser, per parser/README.md's convention */
} LangEntry;

/* Looks up the language for a filename by its extension, or NULL if none of
 * the known languages match. */
const LangEntry *lang_for_ext(const char *filename);

/* The known-language table, for building fs_walk's extension filter without
 * duplicating LANGUAGES here. */
size_t lang_table_count(void);
const LangEntry *lang_table_entry(size_t i);

/* Runs the given language's parser on file_path, capturing its JSON output.
 * Returns 1 and fills *out on success (including a successfully-parsed-but-
 * empty result), 0 if the parser couldn't be run or produced something
 * unparseable. */
int run_parser(const LangEntry *lang, const char *parser_dir, const char *file_path, DxFile *out);

#endif /* ZDOC_DOC_EXTRACTOR_CHILD_PARSER_H */
