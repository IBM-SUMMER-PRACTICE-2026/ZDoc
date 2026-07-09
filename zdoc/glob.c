#include "glob.h"
#include <string.h>

int zdoc_glob_match(const char *pat, const char *str) {
    while (*pat) {
        if (pat[0] == '*' && pat[1] == '*') {
            /* '**' — matches any run of characters, crossing '/'. */
            const char *p = pat;
            while (*p == '*') p++;
            if (*p == '/') {
                /* A double-star followed by a slash may also match zero path
                 * segments, so try skipping the slash as well. */
                if (zdoc_glob_match(p + 1, str)) return 1;
            }
            if (*p == '\0') return 1; /* trailing ** matches the rest */
            for (const char *s = str; ; s++) {
                if (zdoc_glob_match(p, s)) return 1;
                if (*s == '\0') return 0;
            }
        } else if (*pat == '*') {
            /* '*' — matches any run of characters except '/'. */
            const char *p = pat + 1;
            if (*p == '\0') return strchr(str, '/') == NULL;
            for (const char *s = str; ; s++) {
                if (zdoc_glob_match(p, s)) return 1;
                if (*s == '\0' || *s == '/') return 0;
            }
        } else if (*pat == '?') {
            if (*str == '\0' || *str == '/') return 0;
            pat++; str++;
        } else {
            if (*pat != *str) return 0;
            pat++; str++;
        }
    }
    return *str == '\0';
}
