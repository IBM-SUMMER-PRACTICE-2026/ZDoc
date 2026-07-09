#include "cli.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ============================================================
 * String helpers
 * ============================================================ */

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* Replace *dst (owned) with a copy of s. */
static int set_str(char **dst, const char *s) {
    char *copy = xstrdup(s);
    if (s && !copy) return -1;
    free(*dst);
    *dst = copy;
    return 0;
}

static void str_tolower(char *s) {
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

int strlist_push(StrList *l, const char *s) {
    if (l->count == l->cap) {
        size_t cap = l->cap ? l->cap * 2 : 8;
        char **grown = realloc(l->items, cap * sizeof(*grown));
        if (!grown) return -1;
        l->items = grown;
        l->cap = cap;
    }
    char *copy = xstrdup(s);
    if (!copy) return -1;
    l->items[l->count++] = copy;
    return 0;
}

void strlist_free(StrList *l) {
    for (size_t i = 0; i < l->count; i++) free(l->items[i]);
    free(l->items);
    l->items = NULL;
    l->count = l->cap = 0;
}

/* ============================================================
 * Language table
 * ============================================================ */

static const char *EXTS_PLX[]    = { ".plx", ".pls" };
static const char *EXTS_PLAS[]   = { ".plas" };
static const char *EXTS_C[]      = { ".c", ".h" };
static const char *EXTS_CPP[]    = { ".cpp", ".cxx", ".cc", ".hpp" };
static const char *EXTS_JAVA[]   = { ".java" };
static const char *EXTS_ASM[]    = { ".asm", ".s", ".mac" };
static const char *EXTS_PASCAL[] = { ".pas", ".pp" };

typedef struct {
    const char        *name;   /* canonical */
    const char        *alias;  /* accepted alternative, or NULL */
    const char *const *exts;
    size_t             n;
} LangDef;

static const LangDef LANGS[] = {
    { "plx",    NULL,        EXTS_PLX,    2 },
    { "plas",   NULL,        EXTS_PLAS,   1 },
    { "c",      NULL,        EXTS_C,      2 },
    { "cpp",    "c++",       EXTS_CPP,    4 },
    { "java",   NULL,        EXTS_JAVA,   1 },
    { "asm",    "assembler", EXTS_ASM,    3 },
    { "pascal", NULL,        EXTS_PASCAL, 2 },
};
static const size_t LANG_N = sizeof(LANGS) / sizeof(LANGS[0]);

size_t      zdoc_lang_count(void)       { return LANG_N; }
const char *zdoc_lang_name(size_t i)    { return i < LANG_N ? LANGS[i].name : NULL; }

const char *zdoc_lang_canonical(const char *name) {
    for (size_t i = 0; i < LANG_N; i++) {
        if (strcmp(name, LANGS[i].name) == 0) return LANGS[i].name;
        if (LANGS[i].alias && strcmp(name, LANGS[i].alias) == 0) return LANGS[i].name;
    }
    return NULL;
}

int zdoc_lang_known(const char *name) { return zdoc_lang_canonical(name) != NULL; }

static int ext_ieq(const char *a, const char *b) {
    for (; *a && *b; a++, b++)
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
    return *a == *b;
}

const char *zdoc_lang_of_file(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot) return NULL;
    for (size_t i = 0; i < LANG_N; i++)
        for (size_t k = 0; k < LANGS[i].n; k++)
            if (ext_ieq(dot, LANGS[i].exts[k])) return LANGS[i].name;
    return NULL;
}

int zdoc_lang_extensions(const char *canonical, const char **out, size_t cap, size_t *n) {
    for (size_t i = 0; i < LANG_N; i++) {
        if (strcmp(canonical, LANGS[i].name) != 0) continue;
        for (size_t k = 0; k < LANGS[i].n; k++) {
            if (*n >= cap) return -1;
            out[(*n)++] = LANGS[i].exts[k];
        }
        return 0;
    }
    return -1;
}

/* ============================================================
 * Defaults / teardown
 * ============================================================ */

void zdoc_options_init(ZdocOptions *o) {
    memset(o, 0, sizeof(*o));
    o->mode          = ZDOC_MODE_OFFLINE;
    o->output_format = ZDOC_FORMAT_MD;
    o->recursive     = 0;
    o->no_source     = 0;
    o->out_dir       = xstrdup("./zdoc-out");
    o->bob_cli       = xstrdup("bob");
}

void zdoc_options_free(ZdocOptions *o) {
    free(o->out_dir);
    free(o->bob_cli);
    free(o->bob_args);
    free(o->title);
    strlist_free(&o->languages);
    strlist_free(&o->exclude);
    strlist_free(&o->sources);
    memset(o, 0, sizeof(*o));
}

/* ============================================================
 * Scalar value setters shared by config + args
 * ============================================================ */

int zdoc_set_mode(ZdocOptions *o, const char *v) {
    if (strcmp(v, "offline") == 0) { o->mode = ZDOC_MODE_OFFLINE; return 0; }
    if (strcmp(v, "ai") == 0)      { o->mode = ZDOC_MODE_AI;      return 0; }
    return -1;
}

int zdoc_set_format(ZdocOptions *o, const char *v) {
    if (strcmp(v, "md") == 0)   { o->output_format = ZDOC_FORMAT_MD;   return 0; }
    if (strcmp(v, "html") == 0) { o->output_format = ZDOC_FORMAT_HTML; return 0; }
    return -1;
}

/* Accepts true/false/yes/no/on/off/1/0. Returns 0 ok (writes *out), -1 bad. */
int zdoc_parse_bool(const char *v, int *out) {
    if (!strcmp(v, "true") || !strcmp(v, "yes") || !strcmp(v, "on")  || !strcmp(v, "1")) { *out = 1; return 0; }
    if (!strcmp(v, "false")|| !strcmp(v, "no")  || !strcmp(v, "off") || !strcmp(v, "0")) { *out = 0; return 0; }
    return -1;
}

/* ============================================================
 * Argument parsing
 * ============================================================ */

void zdoc_print_usage(FILE *out, const char *prog) {
    fprintf(out,
"ZDoc %s - documentation generator\n"
"\n"
"Usage: %s [options] <source_dir_or_file> [<source_dir_or_file> ...]\n"
"\n"
"Options:\n"
"  --mode offline|ai         Operating mode (default: offline)\n"
"  --output-format md|html   Output format (default: md)\n"
"  --out-dir <path>          Output directory (default: ./zdoc-out)\n"
"  --lang <lang>[,<lang>]    Restrict to listed languages (repeatable)\n"
"  --recursive               Recurse into subdirectories\n"
"  --exclude <glob>          Exclude matching files (repeatable)\n"
"  --bob-cli <path>          Path to the Bob CLI (default: bob)\n"
"  --bob-args <args>         Extra arguments forwarded to Bob CLI\n"
"  --title <string>          Project title shown in the output\n"
"  --no-source               Omit source snippets from output\n"
"  --version                 Print version and exit\n"
"  --help                    Print this help and exit\n"
"\n"
"Supported languages: plx, plas, c, cpp (c++), java, asm (assembler), pascal\n"
"CLI options override values from ./zdoc.yaml.\n",
        ZDOC_VERSION, prog);
}

static void usage_err(const char *prog, const char *fmt, const char *arg) {
    fprintf(stderr, "zdoc: ");
    fprintf(stderr, fmt, arg);
    fprintf(stderr, "\n");
    fprintf(stderr, "Try '%s --help' for usage.\n", prog);
}

int zdoc_parse_args(int argc, char **argv, ZdocOptions *o) {
    const char *prog = (argc > 0 && argv[0]) ? argv[0] : "zdoc";
    int only_positional = 0;
    /* CLI list options replace (not append to) config-provided lists, but only
     * on their first CLI occurrence. */
    int lang_cli_seen = 0, exclude_cli_seen = 0;

    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];

        if (only_positional || arg[0] != '-' || arg[1] == '\0') {
            if (strlist_push(&o->sources, arg) != 0) return -1;
            continue;
        }
        if (strcmp(arg, "--") == 0) { only_positional = 1; continue; }
        if (arg[1] != '-') {
            usage_err(prog, "unknown option '%s' (only long options are supported)", arg);
            return -1;
        }

        /* Split "--name=value". */
        char  name[64];
        char *eq = strchr(arg + 2, '=');
        const char *inl = NULL;
        if (eq) {
            size_t len = (size_t)(eq - (arg + 2));
            if (len >= sizeof(name)) len = sizeof(name) - 1;
            memcpy(name, arg + 2, len);
            name[len] = '\0';
            inl = eq + 1;
        } else {
            snprintf(name, sizeof(name), "%s", arg + 2);
        }

        /* Fetch the value for a value-taking option. */
        #define NEED_VALUE(vptr)                                              \
            do {                                                              \
                if (inl) { (vptr) = inl; }                                    \
                else if (i + 1 < argc) { (vptr) = argv[++i]; }               \
                else { usage_err(prog, "option '--%s' requires a value", name); return -1; } \
            } while (0)

        if (strcmp(name, "help") == 0) { zdoc_print_usage(stdout, prog); return 1; }
        if (strcmp(name, "version") == 0) { printf("zdoc %s\n", ZDOC_VERSION); return 1; }

        if (strcmp(name, "recursive") == 0) {
            if (inl) { if (zdoc_parse_bool(inl, &o->recursive) != 0) {
                usage_err(prog, "invalid boolean for '--recursive': '%s'", inl); return -1; } }
            else o->recursive = 1;
            continue;
        }
        if (strcmp(name, "no-source") == 0) {
            if (inl) { if (zdoc_parse_bool(inl, &o->no_source) != 0) {
                usage_err(prog, "invalid boolean for '--no-source': '%s'", inl); return -1; } }
            else o->no_source = 1;
            continue;
        }

        if (strcmp(name, "mode") == 0) {
            const char *v; NEED_VALUE(v);
            if (zdoc_set_mode(o, v) != 0) { usage_err(prog, "invalid --mode '%s' (offline|ai)", v); return -1; }
            continue;
        }
        if (strcmp(name, "output-format") == 0) {
            const char *v; NEED_VALUE(v);
            if (zdoc_set_format(o, v) != 0) { usage_err(prog, "invalid --output-format '%s' (md|html)", v); return -1; }
            continue;
        }
        if (strcmp(name, "out-dir") == 0)  { const char *v; NEED_VALUE(v); if (set_str(&o->out_dir, v) != 0) return -1; continue; }
        if (strcmp(name, "bob-cli") == 0)  { const char *v; NEED_VALUE(v); if (set_str(&o->bob_cli, v) != 0) return -1; continue; }
        if (strcmp(name, "bob-args") == 0) { const char *v; NEED_VALUE(v); if (set_str(&o->bob_args, v) != 0) return -1; continue; }
        if (strcmp(name, "title") == 0)    { const char *v; NEED_VALUE(v); if (set_str(&o->title, v) != 0) return -1; continue; }

        if (strcmp(name, "lang") == 0) {
            const char *v; NEED_VALUE(v);
            if (!lang_cli_seen) { strlist_free(&o->languages); lang_cli_seen = 1; }
            /* comma-split, normalise, validate each */
            char *dup = xstrdup(v);
            if (!dup) return -1;
            for (char *tok = strtok(dup, ","); tok; tok = strtok(NULL, ",")) {
                str_tolower(tok);
                const char *canon = zdoc_lang_canonical(tok);
                if (!canon) { usage_err(prog, "unknown language '%s'", tok); free(dup); return -1; }
                if (strlist_push(&o->languages, canon) != 0) { free(dup); return -1; }
            }
            free(dup);
            continue;
        }
        if (strcmp(name, "exclude") == 0) {
            const char *v; NEED_VALUE(v);
            if (!exclude_cli_seen) { strlist_free(&o->exclude); exclude_cli_seen = 1; }
            if (strlist_push(&o->exclude, v) != 0) return -1;
            continue;
        }

        usage_err(prog, "unknown option '%s'", arg);
        return -1;
        #undef NEED_VALUE
    }

    return 0;
}

/* ============================================================
 * Validation + reporting
 * ============================================================ */

int zdoc_options_validate(const ZdocOptions *o) {
    if (o->sources.count == 0) {
        fprintf(stderr, "zdoc: no source directory or file given.\n"
                        "Try 'zdoc --help' for usage.\n");
        return -1;
    }
    if (!o->out_dir || o->out_dir[0] == '\0') {
        fprintf(stderr, "zdoc: --out-dir must not be empty.\n");
        return -1;
    }
    for (size_t i = 0; i < o->languages.count; i++) {
        if (!zdoc_lang_known(o->languages.items[i])) {
            fprintf(stderr, "zdoc: unknown language '%s'.\n", o->languages.items[i]);
            return -1;
        }
    }
    return 0;
}

static void print_list(FILE *out, const char *label, const StrList *l, const char *empty) {
    fprintf(out, "  %-14s ", label);
    if (l->count == 0) { fprintf(out, "%s\n", empty); return; }
    for (size_t i = 0; i < l->count; i++)
        fprintf(out, "%s%s", l->items[i], i + 1 < l->count ? ", " : "");
    fprintf(out, "\n");
}

void zdoc_print_options(FILE *out, const ZdocOptions *o) {
    fprintf(out, "ZDoc - resolved configuration\n");
    fprintf(out, "  %-14s %s\n", "mode:",          o->mode == ZDOC_MODE_AI ? "ai" : "offline");
    fprintf(out, "  %-14s %s\n", "output-format:", o->output_format == ZDOC_FORMAT_HTML ? "html" : "md");
    fprintf(out, "  %-14s %s\n", "out-dir:",       o->out_dir);
    fprintf(out, "  %-14s %s\n", "recursive:",     o->recursive ? "yes" : "no");
    print_list(out, "languages:", &o->languages, "(all supported)");
    print_list(out, "exclude:",   &o->exclude,   "(none)");
    fprintf(out, "  %-14s %s\n", "bob-cli:",       o->bob_cli ? o->bob_cli : "(none)");
    fprintf(out, "  %-14s %s\n", "bob-args:",      o->bob_args ? o->bob_args : "(none)");
    fprintf(out, "  %-14s %s\n", "title:",         o->title ? o->title : "(none)");
    fprintf(out, "  %-14s %s\n", "no-source:",     o->no_source ? "yes" : "no");
    print_list(out, "sources:",   &o->sources,    "(none)");
}
