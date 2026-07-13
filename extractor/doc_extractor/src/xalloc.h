/* Error-checked allocation, used throughout doc_extractor. Split out from
 * what used to be json_read.h - the JSON-reading half of that file is now
 * dead (every parser is linked in-process, see parser_link.c; no more
 * subprocess replies to parse) and is quarantined in src/_disabled/. */
#ifndef ZDOC_DOC_EXTRACTOR_XALLOC_H
#define ZDOC_DOC_EXTRACTOR_XALLOC_H

#include <stddef.h>

/* Error-checked allocation - exits the process on OOM. */
void *xmalloc(size_t n);
void *xrealloc(void *p, size_t n);

#endif /* ZDOC_DOC_EXTRACTOR_XALLOC_H */
