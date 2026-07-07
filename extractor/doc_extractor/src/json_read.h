/*
 * Minimal JSON reader scoped to exactly what a ZDoc parser's own JSON
 * output contains (see parser/README.md's contract) - not a general-
 * purpose JSON library. doc_extractor is the one reading it, and the
 * producer (a parser binary we invoke ourselves) is our own code, so this
 * doesn't need to handle input shapes we know we never produce.
 */
#ifndef ZDOC_DOC_EXTRACTOR_JSON_READ_H
#define ZDOC_DOC_EXTRACTOR_JSON_READ_H

#include <stddef.h>

typedef struct {
    const char *p, *end;
    int ok; /* set to 0 the moment anything malformed is seen */
} JParser;

/* Error-checked allocation - exits the process on OOM. */
void *xmalloc(size_t n);
void *xrealloc(void *p, size_t n);

void jskip_ws(JParser *j);
int jpeek(JParser *j);
int jeat(JParser *j, char c);

/* Parse a JSON string literal (the opening quote must be the current char).
 * Returns a freshly-allocated, NUL-terminated decoded string, or NULL and
 * sets j->ok = 0 on malformed input. \uXXXX escapes are decoded as a single
 * byte (cp & 0xFF), not full UTF-8 - sufficient because the only \uXXXX
 * escapes a ZDoc parser's own JSON emitter ever produces are \u00XX for
 * control characters below 0x20, which are single-byte regardless. */
char *jparse_string(JParser *j);

/* Parse a JSON integer (optionally signed; no fractional/exponent part -
 * "line" is always a plain integer). Sets j->ok = 0 on malformed input. */
long jparse_int(JParser *j);

/* Skip over any single JSON value - used to ignore fields this reader
 * doesn't care about (e.g. "zdoc_parser", "version"). */
void jskip_value(JParser *j);

/* Parses "{ "key": <value>, ... }", calling field(j, key, ctx) for each key -
 * the callback is responsible for consuming that key's value. */
typedef void (*ObjectFieldFn)(JParser *j, const char *key, void *ctx);
int parse_object(JParser *j, ObjectFieldFn field, void *ctx);

#endif /* ZDOC_DOC_EXTRACTOR_JSON_READ_H */
