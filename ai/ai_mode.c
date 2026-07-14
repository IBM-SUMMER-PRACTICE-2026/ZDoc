/*
 * ZDoc AI Assisted mode — implementation (see ai_mode.h).
 *
 * For each parsed symbol this resolves the module's source file (the same way
 * the renderers and the daemon do — Module.pathIndex + module_tree, prefixed
 * with fs_walk_root_prefix so it opens from any cwd), slices the symbol's body
 * out of it, assembles the zdoc-diagram snippet, and asks the Bob client to
 * fill in Symbol.diagram.
 *
 * Body slicing here is a deliberate bridge: the parser does not yet expose a
 * per-symbol body or a declaration pool (the one open dependency in
 * ai/AI-FRONT-NOTES.md §2.1). Until it does, we recover the body by cutting the
 * source between a symbol's line and the next symbol's line, and send the
 * function with its doc brief but without the DECLARATIONS/CALLEES context the
 * closure would add. When the parser grows those fields, swap slice_body for
 * the parser's body and feed a real bc_index through bc_closure here — the
 * public interface does not change.
 */
#include "ai_mode.h"

#include "bob_client/closure.h" /* bc_lang, bc_lang_display, bc_build_snippet */
#include "bob_client/util.h"    /* bc_read_file                              */
#include "../extractor/doc_extractor/module_tree/fs_walk.h" /* root prefix   */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* File extension -> diagram language. Mirrors the renderers' small local
 * table; kept here so this layer doesn't depend on a parser binary. */
static bc_lang lang_from_name(const char *name)
{
    const char *dot = name ? strrchr(name, '.') : NULL;
    if (!dot)
        return BC_LANG_UNKNOWN;
    static const struct {
        const char *ext;
        bc_lang lang;
    } tab[] = {
        {".plx", BC_LANG_PLX},   {".pls", BC_LANG_PLX},
        {".plas", BC_LANG_PLAS}, {".c", BC_LANG_C},
        {".h", BC_LANG_C},       {".cpp", BC_LANG_CPP},
        {".cxx", BC_LANG_CPP},   {".cc", BC_LANG_CPP},
        {".hpp", BC_LANG_CPP},   {".java", BC_LANG_JAVA},
        {".asm", BC_LANG_ASM},   {".s", BC_LANG_ASM},
        {".mac", BC_LANG_ASM},   {".pas", BC_LANG_PASCAL},
        {".pp", BC_LANG_PASCAL},
    };
    for (size_t i = 0; i < sizeof tab / sizeof *tab; i++)
        if (strcmp(dot, tab[i].ext) == 0)
            return tab[i].lang;
    return BC_LANG_UNKNOWN;
}

/* Pointer to the first character of 1-based `line` in buf, or NULL if `line`
 * is 0 or lies beyond the end of the buffer. */
static const char *line_start(const char *buf, uint32_t line)
{
    if (line == 0)
        return NULL;
    uint32_t cur = 1;
    const char *p = buf;
    while (cur < line && *p) {
        if (*p == '\n')
            cur++;
        p++;
    }
    return (cur == line) ? p : NULL;
}

/* Slice symbol `si`'s body out of `buf`: from its own line to the start of the
 * next symbol's line (whichever symbol begins soonest after it), or to EOF for
 * the last one. malloc'd, trailing whitespace trimmed. NULL if the line can't
 * be located or on allocation failure. */
static char *slice_body(const char *buf, const Module *mod, int si)
{
    uint32_t line = mod->symbols[si].line;
    const char *start = line_start(buf, line);
    if (!start)
        return NULL;

    uint32_t next = 0; /* 0 => no later symbol; slice to EOF */
    for (int j = 0; j < mod->symbolCount; j++) {
        uint32_t l = mod->symbols[j].line;
        if (l > line && (next == 0 || l < next))
            next = l;
    }

    const char *end = NULL;
    if (next)
        end = line_start(buf, next);
    if (!end || end <= start)
        end = buf + strlen(buf);

    size_t n = (size_t)(end - start);
    char *body = malloc(n + 1);
    if (!body)
        return NULL;
    memcpy(body, start, n);
    while (n > 0 && (body[n - 1] == '\n' || body[n - 1] == '\r' ||
                     body[n - 1] == '\t' || body[n - 1] == ' '))
        n--;
    body[n] = '\0';
    if (n == 0) {
        free(body);
        return NULL;
    }
    return body;
}

/* Build the openable source path for file index `pi`, mirroring the daemon:
 * fs_walk_root_prefix + "/" + reconstructed relative path. Returns 0 on
 * success. */
static int resolve_source_path(const modtree_dir_table_t *dirs,
                               const modtree_file_table_t *files, int pi,
                               char *out, size_t out_size)
{
    char rel[FS_WALK_PATH_MAX];
    if (modtree_file_path(dirs, files, pi, rel, sizeof rel) != 0)
        return -1;
    int need;
    if (fs_walk_root_prefix[0])
        need = snprintf(out, out_size, "%s/%s", fs_walk_root_prefix, rel);
    else
        need = snprintf(out, out_size, "%s", rel);
    return (need > 0 && (size_t)need < out_size) ? 0 : -1;
}

/* Core per-symbol pass, shared by both entry points: read `path` once, then for
 * each symbol slice its body by starting line, build the snippet, and call Bob.
 * The sliced diagram lands in each symbol's `diagram`, so a diagram is tied to
 * its symbol by that symbol's starting line. Returns the count annotated. */
static int annotate_from_source(const char *path, bc_lang lang, Module *mod,
                                const AiOptions *opt)
{
    if (!path || !mod || mod->symbolCount <= 0)
        return 0;

    const char *display = bc_lang_display(lang);

    size_t flen = 0;
    char *buf = bc_read_file(path, &flen);
    if (!buf)
        return 0;

    int done = 0;
    for (int i = 0; i < mod->symbolCount; i++) {
        Symbol *sym = &mod->symbols[i];
        char *body = slice_body(buf, mod, i);
        if (!body)
            continue;
        /* No declaration closure yet (parser exposes no decl pool): send the
         * doc brief + the function body. Empty sections are omitted. */
        char *snippet = bc_build_snippet(sym->description, NULL, 0, NULL, 0,
                                         lang, body);
        free(body);
        if (!snippet)
            continue;
        if (bob_annotate(&opt->bob, display, snippet, sym) == 0)
            done++;
        free(snippet);
    }

    free(buf);
    return done;
}

/* Annotate one module resolved through the module_tree tables (batch path). */
static int annotate_module(const modtree_dir_table_t *dirs,
                           const modtree_file_table_t *files, Module *mod,
                           const AiOptions *opt)
{
    int pi = mod->pathIndex;
    if (pi < 0 || (size_t)pi >= files->count || mod->symbolCount <= 0)
        return 0;

    char path[FS_WALK_PATH_MAX + 512];
    if (resolve_source_path(dirs, files, pi, path, sizeof path) != 0)
        return 0;

    return annotate_from_source(path, lang_from_name(files->files[pi].name),
                                mod, opt);
}

int zdoc_ai_annotate_file(const char *path, Module *mod, const AiOptions *opt)
{
    if (!path || !mod || !opt)
        return -1;
    return annotate_from_source(path, lang_from_name(path), mod, opt);
}

int zdoc_ai_annotate(const modtree_dir_table_t *dirs,
                     const modtree_file_table_t *files, Module *modules,
                     size_t module_count, const AiOptions *opt)
{
    if (!dirs || !files || !modules || !opt)
        return -1;

    int total = 0;
    for (size_t m = 0; m < module_count; m++)
        total += annotate_module(dirs, files, &modules[m], opt);
    return total;
}
