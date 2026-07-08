/*
 * Minimal hand-rolled JSON reader - just enough to parse the
 * documentation-model schema md_model.c consumes. Not a general-purpose
 * JSON library; internal to md_renderer, not part of its public API
 * (see md_renderer.h for that).
 */
#ifndef ZDOC_MD_RENDERER_JSON_H
#define ZDOC_MD_RENDERER_JSON_H

#include <stddef.h>

typedef struct {
    const char *p, *end;
    int ok; /* set to 0 the moment anything malformed is seen */
} JParser;

/* Error-checked allocation - exits the process on OOM, same convention as
 * every other ZDoc C component. */
void *xmalloc(size_t n);
void *xrealloc(void *p, size_t n);

void jskip_ws(JParser *j);
int jpeek(JParser *j);
int jeat(JParser *j, char c);

/* Parse a JSON string literal (the opening quote must be the current char).
 * Returns a freshly-allocated, NUL-terminated decoded string, or NULL and
 * sets j->ok = 0 on malformed input. */
char *jparse_string(JParser *j);

/* Parse a JSON integer (optionally signed; no fractional/exponent part -
 * see json.c for why). Sets j->ok = 0 on malformed input. */
long jparse_int(JParser *j);

/* Skip over any single JSON value - used to ignore keys/entries the caller
 * doesn't recognize, so unknown fields don't break parsing. */
void jskip_value(JParser *j);

/* Parses "{ "key": <value>, ... }", calling field(j, key, ctx) for each key -
 * the callback is responsible for consuming that key's value (via a jparse_*
 * call, a nested parse_object, or jskip_value for anything it doesn't
 * recognize). */
typedef void (*ObjectFieldFn)(JParser *j, const char *key, void *ctx);
int parse_object(JParser *j, ObjectFieldFn field, void *ctx);

#endif /* ZDOC_MD_RENDERER_JSON_H */
