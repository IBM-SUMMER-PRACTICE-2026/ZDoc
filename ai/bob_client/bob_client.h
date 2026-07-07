/* Provider invocation with timeout, content-addressed cache, one retry,
 * and record/replay support. See docs/zdoc-ai-mode.md. */
#ifndef ZDOC_BC_BOB_CLIENT_H
#define ZDOC_BC_BOB_CLIENT_H

#include <stddef.h>

#define BC_SKILL_VERSION "1.0.0" /* must match .bob/skills/zdoc-diagram */

typedef struct {
    const char *command_tmpl; /* "bob explain --diagram --brief --lang {lang}" */
    int timeout_sec;          /* per call; default 60 */
    int no_cache;             /* disable cache entirely */
    int refresh;              /* skip cache reads, still write */
    const char *cache_dir;    /* NULL -> ~/.cache/zdoc */
    const char *record_dir;   /* NULL -> no recording */
} bc_cfg;

typedef struct {
    char *mermaid;   /* malloc'd; NULL on failure */
    char **calls;    /* malloc'd array of malloc'd call-node texts */
    size_t ncalls;
    int cached;      /* served from cache */
    int repaired;    /* response needed normalization / mermaid fallback */
    int attempts;    /* provider invocations made (0 when cached) */
    char *errmsg;    /* malloc'd on failure */
    char *raw;       /* malloc'd, truncated raw response (for the fail log) */
} bc_result;

/* Generate one diagram. Returns 0 on success (result->mermaid set),
 * -1 on failure (result->errmsg set). Thread-safe. */
int bc_generate(const bc_cfg *cfg, const char *lang_name,
                const char *snippet, bc_result *out);

void bc_result_free(bc_result *r);

/* Resolved cache directory (expands the default); malloc'd. */
char *bc_cache_dir(const bc_cfg *cfg);

#endif
