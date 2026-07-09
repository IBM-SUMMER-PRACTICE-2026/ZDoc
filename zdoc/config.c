#include "cli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Minimal zdoc.yaml reader — a deliberate subset, not a real YAML parser.
 * Supports:
 *   key: value            scalar (quoted or bare; bare values may carry a
 *                         trailing " # comment")
 *   key:                  starts a list; following "- item" lines are members
 *   - item                list member (for `languages` and `exclude`)
 *   # ...                 full-line or trailing comment
 * Unknown keys are warned about and skipped. Values feed the same setters the
 * CLI uses, so precedence stays: defaults < config < CLI. */

static char *dup_owned(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static int assign(char **dst, const char *v) {
    char *copy = dup_owned(v);
    if (v && !copy) return -1;
    free(*dst);
    *dst = copy;
    return 0;
}

/* Trim leading and trailing ASCII whitespace in place; returns start. */
static char *trim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) *--end = '\0';
    return s;
}

/* Resolve a scalar value: strip surrounding quotes, or (bare) drop a trailing
 * " # comment". Returns a pointer into `v`. */
static char *clean_scalar(char *v) {
    v = trim(v);
    if (*v == '"' || *v == '\'') {
        char q = *v++;
        char *close = strchr(v, q);
        if (close) *close = '\0';
        return v;
    }
    char *hash = strstr(v, " #");
    if (hash) { *hash = '\0'; v = trim(v); }
    return v;
}

int zdoc_config_load(const char *path, ZdocOptions *o) {
    FILE *f = fopen(path, "r");
    if (!f) return 0; /* missing/unreadable config is not an error */

    char line[4096];
    int lineno = 0;
    int rc = 0;
    enum { LIST_NONE, LIST_LANG, LIST_EXCLUDE } cur_list = LIST_NONE;

    while (fgets(line, sizeof(line), f)) {
        lineno++;
        line[strcspn(line, "\r\n")] = '\0';

        char *p = line;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p == '\0' || *p == '#') continue; /* blank / comment */

        /* list member */
        if (*p == '-' && (p[1] == ' ' || p[1] == '\0')) {
            char *item = clean_scalar(p + 1);
            if (cur_list == LIST_LANG) {
                char low[64];
                snprintf(low, sizeof(low), "%s", item);
                for (char *c = low; *c; c++) *c = (char)tolower((unsigned char)*c);
                const char *canon = zdoc_lang_canonical(low);
                if (!canon) {
                    fprintf(stderr, "zdoc: %s:%d: unknown language '%s'\n", path, lineno, item);
                    rc = -1; break;
                }
                if (strlist_push(&o->languages, canon) != 0) { rc = -1; break; }
            } else if (cur_list == LIST_EXCLUDE) {
                if (strlist_push(&o->exclude, item) != 0) { rc = -1; break; }
            } else {
                fprintf(stderr, "zdoc: %s:%d: list item outside a list, ignored\n", path, lineno);
            }
            continue;
        }

        /* key: value */
        char *colon = strchr(p, ':');
        if (!colon) {
            fprintf(stderr, "zdoc: %s:%d: expected 'key: value', ignored\n", path, lineno);
            continue;
        }
        *colon = '\0';
        char *key = trim(p);
        char *val = colon + 1;
        while (*val && isspace((unsigned char)*val)) val++;
        int has_val = (*val != '\0' && *val != '#');

        cur_list = LIST_NONE;

        if (strcmp(key, "languages") == 0) {
            strlist_free(&o->languages); /* config replaces */
            cur_list = LIST_LANG;
            if (has_val) fprintf(stderr, "zdoc: %s:%d: 'languages' expects a list\n", path, lineno);
            continue;
        }
        if (strcmp(key, "exclude") == 0) {
            strlist_free(&o->exclude);
            cur_list = LIST_EXCLUDE;
            if (has_val) fprintf(stderr, "zdoc: %s:%d: 'exclude' expects a list\n", path, lineno);
            continue;
        }

        if (!has_val) {
            fprintf(stderr, "zdoc: %s:%d: '%s' has no value, ignored\n", path, lineno, key);
            continue;
        }
        char *v = clean_scalar(val);

        if (strcmp(key, "title") == 0)        { if (assign(&o->title,   v) != 0) { rc = -1; break; } }
        else if (strcmp(key, "out_dir") == 0) { if (assign(&o->out_dir, v) != 0) { rc = -1; break; } }
        else if (strcmp(key, "bob_cli") == 0) { if (assign(&o->bob_cli, v) != 0) { rc = -1; break; } }
        else if (strcmp(key, "bob_args") == 0){ if (assign(&o->bob_args,v) != 0) { rc = -1; break; } }
        else if (strcmp(key, "mode") == 0) {
            if (zdoc_set_mode(o, v) != 0) {
                fprintf(stderr, "zdoc: %s:%d: invalid mode '%s' (offline|ai)\n", path, lineno, v);
                rc = -1; break;
            }
        }
        else if (strcmp(key, "output_format") == 0) {
            if (zdoc_set_format(o, v) != 0) {
                fprintf(stderr, "zdoc: %s:%d: invalid output_format '%s' (md|html)\n", path, lineno, v);
                rc = -1; break;
            }
        }
        else if (strcmp(key, "recursive") == 0) {
            if (zdoc_parse_bool(v, &o->recursive) != 0) {
                fprintf(stderr, "zdoc: %s:%d: invalid recursive '%s' (true|false)\n", path, lineno, v);
                rc = -1; break;
            }
        }
        else {
            fprintf(stderr, "zdoc: %s:%d: unknown key '%s', ignored\n", path, lineno, key);
        }
    }

    fclose(f);
    return rc;
}
