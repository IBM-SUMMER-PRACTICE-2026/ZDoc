#include "child_parser.h"
#include "json_read.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
/* No fork()/execvp() on Windows - fall back to _popen, which still goes
 * through a shell and so keeps the same embedded-quote caveat noted below. */
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

/* --------------------------------------------------- language lookup table */

static const LangEntry LANGUAGES[] = {
    { ".java", "java", "zdoc-java-parser" },
    { ".c",    "c",    "zdoc-c-parser" },
    { ".h",    "c",    "zdoc-c-parser" },
    { ".cpp",  "cpp",  "zdoc-c-parser" },
    { ".cxx",  "cpp",  "zdoc-c-parser" },
    { ".cc",   "cpp",  "zdoc-c-parser" },
    { ".hpp",  "cpp",  "zdoc-c-parser" },
    { ".plx",  "plx",  "zdoc-plx-parser" },
    { ".pls",  "plx",  "zdoc-plx-parser" },
    { ".plas", "plas", "zdoc-plx-parser" },
};
#define LANGUAGE_COUNT (sizeof LANGUAGES / sizeof *LANGUAGES)

const LangEntry *lang_for_ext(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if(!dot) return NULL;
    for(size_t i = 0; i < LANGUAGE_COUNT; i++)
        if(strcmp(dot, LANGUAGES[i].ext) == 0) return &LANGUAGES[i];
    return NULL;
}

size_t lang_table_count(void) { return LANGUAGE_COUNT; }
const LangEntry *lang_table_entry(size_t i) { return &LANGUAGES[i]; }

/* ------------------------------------------------------------ JSON output */

static char *read_all(FILE *f, size_t *out_len) {
    size_t cap = 1 << 16, len = 0;
    char *buf = xmalloc(cap);
    size_t n;
    while((n = fread(buf + len, 1, cap - len, f)) > 0) {
        len += n;
        if(len == cap) { cap *= 2; buf = xrealloc(buf, cap); }
    }
    *out_len = len;
    return buf;
}

/* Parses one parser's own JSON output (parser/README.md's contract:
 * {"zdoc_parser","version","modules":[{"file","language","symbols":[...]}]}).
 * A batch invocation gets back one "modules" entry per file passed in, so
 * every one of them is collected (not just the first) and matched back to
 * the right target afterward by its "file" value. */
static void field_param(JParser *j, const char *key, void *ctx) {
    DxParam *out = ctx;
    if(strcmp(key, "name") == 0) out->name = jparse_string(j);
    else if(strcmp(key, "desc") == 0) out->desc = jparse_string(j);
    else jskip_value(j);
}

static void field_doc(JParser *j, const char *key, void *ctx) {
    DxSymbol *sym = ctx;
    if(strcmp(key, "brief") == 0) {
        sym->brief = jparse_string(j);
    } else if(strcmp(key, "returns") == 0) {
        sym->returns = jparse_string(j);
    } else if(strcmp(key, "notes") == 0) {
        sym->notes = jparse_string(j);
    } else if(strcmp(key, "params") == 0) {
        if(!jeat(j, '[')) return;
        size_t cap = 0;
        if(jpeek(j) != ']') {
            for(;;) {
                if(sym->param_count == cap) {
                    cap = cap ? cap * 2 : 4;
                    sym->params = xrealloc(sym->params, cap * sizeof(DxParam));
                }
                DxParam *p = &sym->params[sym->param_count];
                memset(p, 0, sizeof *p);
                if(!parse_object(j, field_param, p)) return;
                sym->param_count++;
                if(jpeek(j) == ',') { jeat(j, ','); continue; }
                break;
            }
        }
        jeat(j, ']');
    } else {
        jskip_value(j);
    }
}

static void field_symbol(JParser *j, const char *key, void *ctx) {
    DxSymbol *out = ctx;
    if(strcmp(key, "kind") == 0) out->kind = jparse_string(j);
    else if(strcmp(key, "name") == 0) out->name = jparse_string(j);
    else if(strcmp(key, "signature") == 0) out->signature = jparse_string(j);
    else if(strcmp(key, "line") == 0) out->line = (uint32_t)jparse_int(j);
    else if(strcmp(key, "doc") == 0) parse_object(j, field_doc, out);
    else jskip_value(j);
}

/* One parsed "modules" entry, before it's matched back to the DxFile that
 * requested it. Mirrors DxFile's doc-bearing fields plus the "file" value
 * (the exact path we passed as an argument) used for that matching. */
typedef struct {
    char     *file;
    char     *language;
    DxSymbol *symbols;
    size_t    symbol_count;
} BatchModule;

static void free_batch_module_contents(BatchModule *m) {
    free(m->file);
    free(m->language);
    for(size_t k = 0; k < m->symbol_count; k++) dx_free_symbol(&m->symbols[k]);
    free(m->symbols);
}

static void field_batch_module(JParser *j, const char *key, void *ctx) {
    BatchModule *out = ctx;
    if(strcmp(key, "file") == 0) {
        out->file = jparse_string(j);
    } else if(strcmp(key, "language") == 0) {
        out->language = jparse_string(j);
    } else if(strcmp(key, "symbols") == 0) {
        if(!jeat(j, '[')) return;
        size_t cap = 0;
        if(jpeek(j) != ']') {
            for(;;) {
                if(out->symbol_count == cap) {
                    cap = cap ? cap * 2 : 8;
                    out->symbols = xrealloc(out->symbols, cap * sizeof(DxSymbol));
                }
                DxSymbol *s = &out->symbols[out->symbol_count];
                memset(s, 0, sizeof *s);
                if(!parse_object(j, field_symbol, s)) return;
                out->symbol_count++;
                if(jpeek(j) == ',') { jeat(j, ','); continue; }
                break;
            }
        }
        jeat(j, ']');
    } else {
        jskip_value(j);
    }
}

typedef struct {
    BatchModule *modules;
    size_t count, cap;
} BatchTopCtx;

static void field_batch_top(JParser *j, const char *key, void *ctx) {
    BatchTopCtx *tc = ctx;
    if(strcmp(key, "modules") != 0) { jskip_value(j); return; }

    if(!jeat(j, '[')) return;
    if(jpeek(j) != ']') {
        for(;;) {
            if(tc->count == tc->cap) {
                tc->cap = tc->cap ? tc->cap * 2 : 8;
                tc->modules = xrealloc(tc->modules, tc->cap * sizeof(BatchModule));
            }
            BatchModule *m = &tc->modules[tc->count];
            memset(m, 0, sizeof *m);
            if(!parse_object(j, field_batch_module, m)) return;
            tc->count++;
            if(jpeek(j) == ',') { jeat(j, ','); continue; }
            break;
        }
    }
    jeat(j, ']');
}

static int parse_batch_output(const char *json, size_t len, BatchTopCtx *tc) {
    JParser jp = { json, json + len, 1 };
    return parse_object(&jp, field_batch_top, tc);
}

/* Matches every parsed module back to the path/target that produced it
 * (by exact string comparison against the "file" value each parser echoes
 * back verbatim), moving each match's language/symbols into the target and
 * clearing its error flag. Modules are freed either way - anything moved
 * into a target has its BatchModule fields nulled out first so freeing the
 * batch array afterward doesn't also free what the target now owns. */
static void distribute_batch_results(BatchTopCtx *tc, const char *const *paths,
                                      DxFile **targets, size_t count) {
    for(size_t m = 0; m < tc->count; m++) {
        for(size_t i = 0; i < count; i++) {
            if(tc->modules[m].file && strcmp(tc->modules[m].file, paths[i]) == 0) {
                targets[i]->language = tc->modules[m].language;
                targets[i]->symbols = tc->modules[m].symbols;
                targets[i]->symbol_count = tc->modules[m].symbol_count;
                targets[i]->error = 0;
                tc->modules[m].language = NULL;
                tc->modules[m].symbols = NULL;
                tc->modules[m].symbol_count = 0;
                break;
            }
        }
        free_batch_module_contents(&tc->modules[m]);
    }
    free(tc->modules);
}

/* Runs bin (via -parser_dir or PATH) once, with every path in paths[] as a
 * separate argument, and distributes the results. Returns 1 if the
 * invocation itself succeeded (ran, exited 0, produced parseable JSON) -
 * that does not guarantee every target got matched (see
 * distribute_batch_results); returns 0 if the whole invocation failed, in
 * which case no targets are touched at all. */
#ifdef _WIN32

/* Windows has no fork()/execvp() - fall back to _popen, built from a quoted
 * command string. Same caveat as before applies here only: tolerates spaces
 * in paths, not embedded quote characters or other shell metacharacters. */
int run_parser_batch(const LangEntry *lang, const char *parser_dir,
                      const char *const *paths, DxFile **targets, size_t count) {
    char bin[512];
    if(parser_dir) snprintf(bin, sizeof bin, "%s/%s", parser_dir, lang->parser_bin);
    else snprintf(bin, sizeof bin, "%s", lang->parser_bin);

    char cmd[8192];
    int n = snprintf(cmd, sizeof cmd, "\"%s\"", bin);
    for(size_t i = 0; i < count && (size_t)n < sizeof cmd; i++)
        n += snprintf(cmd + n, sizeof cmd - (size_t)n, " \"%s\"", paths[i]);

    FILE *pipe = _popen(cmd, "r");
    if(!pipe) return 0;

    size_t len;
    char *output = read_all(pipe, &len);
    int status = _pclose(pipe);

    if(status != 0) { free(output); return 0; }

    BatchTopCtx tc = {0};
    int ok = parse_batch_output(output, len, &tc);
    free(output);
    if(!ok) {
        for(size_t m = 0; m < tc.count; m++) free_batch_module_contents(&tc.modules[m]);
        free(tc.modules);
        return 0;
    }

    distribute_batch_results(&tc, paths, targets, count);
    return 1;
}

#else

/* Uses fork()+pipe()+execvp() instead of popen()/a shell command string -
 * every path is passed as a real argv entry, never interpreted by a shell,
 * so spaces/quotes/any other character in a path just work with no
 * escaping needed at all. */
int run_parser_batch(const LangEntry *lang, const char *parser_dir,
                      const char *const *paths, DxFile **targets, size_t count) {
    char bin[512];
    if(parser_dir) snprintf(bin, sizeof bin, "%s/%s", parser_dir, lang->parser_bin);
    else snprintf(bin, sizeof bin, "%s", lang->parser_bin);

    char **argv = xmalloc((count + 2) * sizeof(char *));
    argv[0] = bin;
    for(size_t i = 0; i < count; i++) argv[1 + i] = (char *)paths[i];
    argv[1 + count] = NULL;

    int fds[2];
    if(pipe(fds) != 0) { free(argv); return 0; }

    pid_t pid = fork();
    if(pid < 0) { close(fds[0]); close(fds[1]); free(argv); return 0; }

    if(pid == 0) {
        /* child: stdout -> the pipe, then replace ourselves with the parser */
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        close(fds[1]);
        execvp(bin, argv);
        _exit(127); /* only reached if execvp itself failed */
    }

    /* parent */
    free(argv);
    close(fds[1]);
    FILE *stream = fdopen(fds[0], "r");
    if(!stream) { close(fds[0]); waitpid(pid, NULL, 0); return 0; }

    size_t len;
    char *output = read_all(stream, &len);
    fclose(stream);

    int status;
    waitpid(pid, &status, 0);

    if(!WIFEXITED(status) || WEXITSTATUS(status) != 0) { free(output); return 0; }

    BatchTopCtx tc = {0};
    int ok = parse_batch_output(output, len, &tc);
    free(output);
    if(!ok) {
        for(size_t m = 0; m < tc.count; m++) free_batch_module_contents(&tc.modules[m]);
        free(tc.modules);
        return 0;
    }

    distribute_batch_results(&tc, paths, targets, count);
    return 1;
}

#endif
