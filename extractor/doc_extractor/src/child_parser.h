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

/* Runs the given language's parser ONCE on all 'count' files in paths[],
 * instead of once per file - the parsers already support multiple file
 * arguments in a single invocation, each getting its own "modules" entry
 * in the JSON reply. This amortizes process-spawn overhead across the
 * whole batch instead of paying it per file.
 *
 * targets[i] corresponds to paths[i] and is filled in on success (language,
 * symbols, error cleared to 0) by matching each returned module's "file"
 * value back to the path that produced it. If the invocation itself fails
 * (parser missing/crashed/unparseable output), returns 0 and leaves every
 * targets[i] untouched - the caller is responsible for marking those as
 * errors. If the invocation succeeds but a particular file's module is
 * missing from the reply (shouldn't normally happen), that target is also
 * left untouched so the caller can still detect and flag it. */
int run_parser_batch(const LangEntry *lang, const char *parser_dir,
                      const char *const *paths, DxFile **targets, size_t count);

#endif /* ZDOC_DOC_EXTRACTOR_CHILD_PARSER_H */
