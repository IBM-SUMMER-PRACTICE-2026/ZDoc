/*
 * PL/X is linked in directly (parser/plx_parser/plx_parser.c is compiled
 * into this binary) instead of run as a subprocess - see child_parser.h's
 * ParseMode. This is the one adapter that converts the PL/X parser's own
 * Module/Symbol shape into doc_extractor's DxFile/DxSymbol.
 */
#ifndef ZDOC_DOC_EXTRACTOR_PLX_LINK_H
#define ZDOC_DOC_EXTRACTOR_PLX_LINK_H

#include <stddef.h>

#include "doc_extractor.h"

/* Calls plx_parse_file() directly (in-process, no subprocess) once per file
 * in paths[], filling targets[i] on success. Unlike the subprocess path,
 * each file here is independent - one file's parse failing (plx_parse_file
 * returns NULL, e.g. the file couldn't be opened) only leaves that target
 * untouched; it never affects the other files in the same batch. Always
 * returns 1 - there is no whole-batch failure mode to report. */
int run_plx_linked_batch(const char *const *paths, DxFile **targets, size_t count);

#endif /* ZDOC_DOC_EXTRACTOR_PLX_LINK_H */
