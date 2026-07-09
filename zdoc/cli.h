#ifndef ZDOC_CLI_H
#define ZDOC_CLI_H

/* zdoc CLI front-end: option/tag parsing, zdoc.yaml (subset) loading, and the
 * resolved-options model the rest of the pipeline consumes. See docs/ZDOC.md
 * -> "CLI Usage" and "Configuration File". */

#include <stddef.h>
#include <stdio.h>

#define ZDOC_VERSION "0.1.0"

typedef enum { ZDOC_MODE_OFFLINE = 0, ZDOC_MODE_AI } ZdocMode;
typedef enum { ZDOC_FORMAT_MD = 0, ZDOC_FORMAT_HTML } ZdocFormat;

/* Owned, growable list of heap strings. */
typedef struct {
    char  **items;
    size_t  count;
    size_t  cap;
} StrList;

int  strlist_push(StrList *l, const char *s); /* copies s; 0 ok, -1 OOM */
void strlist_free(StrList *l);

/* The fully-resolved run configuration (defaults < zdoc.yaml < CLI flags). */
typedef struct {
    ZdocMode   mode;
    ZdocFormat output_format;
    char      *out_dir;    /* owned */
    StrList    languages;  /* canonical lang names; empty = all supported */
    int        recursive;
    StrList    exclude;    /* glob patterns */
    char      *bob_cli;    /* owned */
    char      *bob_args;   /* owned, may be NULL */
    char      *title;      /* owned, may be NULL */
    int        no_source;
    StrList    sources;    /* positional source dirs/files (>= 1 required) */
} ZdocOptions;

void zdoc_options_init(ZdocOptions *o); /* spec defaults */
void zdoc_options_free(ZdocOptions *o);

/* Scalar setters shared by config + argument parsing. Each returns 0 on a
 * valid value, -1 otherwise. */
int  zdoc_set_mode(ZdocOptions *o, const char *v);   /* "offline" | "ai" */
int  zdoc_set_format(ZdocOptions *o, const char *v); /* "md" | "html" */
int  zdoc_parse_bool(const char *v, int *out);       /* true/false/yes/no/1/0 */

/* Load a zdoc.yaml (subset) at `path` into o. A missing file is not an error
 * (returns 0). Returns 0 on success, -1 on IO/parse error (message on stderr). */
int  zdoc_config_load(const char *path, ZdocOptions *o);

/* Parse argv (argv[0] = program name) into o, overriding whatever config set.
 * Returns:
 *    0  parsed OK, continue
 *    1  handled --help / --version (already printed) — caller should exit 0
 *   -1  parse error (usage printed to stderr) — caller should exit 2          */
int  zdoc_parse_args(int argc, char **argv, ZdocOptions *o);

/* Validate a resolved config (>= 1 source, known language names, non-empty
 * out-dir). Returns 0 ok, -1 error (message on stderr). */
int  zdoc_options_validate(const ZdocOptions *o);

void zdoc_print_usage(FILE *out, const char *prog);
void zdoc_print_options(FILE *out, const ZdocOptions *o); /* resolved dump */

/* ---- language table (used by discovery in main.c) ---- */

/* Canonical language name for a filename by its extension, or NULL if the
 * extension maps to no supported language. Case-insensitive on the extension. */
const char *zdoc_lang_of_file(const char *filename);

/* 1 if `name` (or a known alias, e.g. "assembler"->"asm") is a supported
 * language, else 0. */
int zdoc_lang_known(const char *name);

/* Canonical name for `name`/alias, or NULL if unknown. */
const char *zdoc_lang_canonical(const char *name);

/* Number of supported languages and the i-th canonical name (for "all"). */
size_t      zdoc_lang_count(void);
const char *zdoc_lang_name(size_t i);

/* Append every extension of language `canonical` (incl. leading dot) to `out`
 * (a caller array of capacity `cap`), advancing *n. Returns 0 ok, -1 if full or
 * unknown language. */
int zdoc_lang_extensions(const char *canonical, const char **out, size_t cap, size_t *n);

#endif
