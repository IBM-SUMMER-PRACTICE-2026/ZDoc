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

/* Only Java has a merged, usable parser right now (Pascal's work is stopped;
 * C's parser exists but isn't wired in here yet) - adding a language later
 * is one more row. */
static const LangEntry LANGUAGES[] = {
    { ".java", "java", "zdoc-java-parser" },
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
 * {"zdoc_parser","version","modules":[{"file","language","symbols":[...]}]})
 * and fills out->language/out->symbols/out->symbol_count from its first
 * module entry - there is exactly one, since the parser is invoked on one
 * file at a time. out->name/parent_dir_index are left untouched; the
 * caller sets those from module_tree, not from the parser's own output. */
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

static void field_module(JParser *j, const char *key, void *ctx) {
    DxFile *out = ctx;
    if(strcmp(key, "language") == 0) {
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
        jskip_value(j); /* "file", or anything else we don't need */
    }
}

typedef struct {
    DxFile *target;
    int found_module;
} TopCtx;

static void field_top(JParser *j, const char *key, void *ctx) {
    TopCtx *tc = ctx;
    if(strcmp(key, "modules") != 0) { jskip_value(j); return; }

    if(!jeat(j, '[')) return;
    if(jpeek(j) != ']') {
        for(;;) {
            if(!tc->found_module) {
                if(!parse_object(j, field_module, tc->target)) return;
                tc->found_module = 1;
            } else {
                jskip_value(j); /* ignore any modules after the first - shouldn't happen */
            }
            if(jpeek(j) == ',') { jeat(j, ','); continue; }
            break;
        }
    }
    jeat(j, ']');
}

static int parse_child_output(const char *json, size_t len, DxFile *out) {
    JParser jp = { json, json + len, 1 };
    TopCtx tc = { out, 0 };
    return parse_object(&jp, field_top, &tc);
}

/* ------------------------------------------------------ parser invocation */

#ifdef _WIN32

/* Windows has no fork()/execvp() - fall back to _popen, built from a quoted
 * command string. Same caveat as before applies here only: tolerates spaces
 * in paths, not embedded quote characters or other shell metacharacters. */
int run_parser(const LangEntry *lang, const char *parser_dir,
                const char *file_path, DxFile *out) {
    char bin[512];
    if(parser_dir) snprintf(bin, sizeof bin, "%s/%s", parser_dir, lang->parser_bin);
    else snprintf(bin, sizeof bin, "%s", lang->parser_bin);

    char cmd[1200];
    /* cmd.exe strips the first and last quote from the /c command string, so
     * with both the binary and the file quoted it must be wrapped in one
     * extra outer pair or the program name keeps a stray embedded quote. */
    snprintf(cmd, sizeof cmd, "\"\"%s\" \"%s\"\"", bin, file_path);

    FILE *pipe = _popen(cmd, "r");
    if(!pipe) return 0;

    size_t len;
    char *output = read_all(pipe, &len);
    int status = _pclose(pipe);

    if(status != 0) { free(output); return 0; }

    int ok = parse_child_output(output, len, out);
    free(output);
    return ok;
}

#else

/* Uses fork()+pipe()+execlp() instead of popen()/a shell command string -
 * file_path is passed as a real argv entry, never interpreted by a shell,
 * so spaces/quotes/any other character in a path just work with no
 * escaping needed at all. */
int run_parser(const LangEntry *lang, const char *parser_dir,
                const char *file_path, DxFile *out) {
    char bin[512];
    if(parser_dir) snprintf(bin, sizeof bin, "%s/%s", parser_dir, lang->parser_bin);
    else snprintf(bin, sizeof bin, "%s", lang->parser_bin);

    int fds[2];
    if(pipe(fds) != 0) return 0;

    pid_t pid = fork();
    if(pid < 0) { close(fds[0]); close(fds[1]); return 0; }

    if(pid == 0) {
        /* child: stdout -> the pipe, then replace ourselves with the parser */
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        close(fds[1]);
        execlp(bin, bin, file_path, (char *)NULL);
        _exit(127); /* only reached if execlp itself failed */
    }

    /* parent */
    close(fds[1]);
    FILE *stream = fdopen(fds[0], "r");
    if(!stream) { close(fds[0]); waitpid(pid, NULL, 0); return 0; }

    size_t len;
    char *output = read_all(stream, &len);
    fclose(stream);

    int status;
    waitpid(pid, &status, 0);

    if(!WIFEXITED(status) || WEXITSTATUS(status) != 0) { free(output); return 0; }

    int ok = parse_child_output(output, len, out);
    free(output);
    return ok;
}

#endif
