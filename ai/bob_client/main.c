/*
 * zdoc-bob-client — AI Assisted mode filter.
 *
 * Reads the shared parser JSON (with --ai-context data) on stdin or from
 * --input, generates one block diagram per function/procedure/entry symbol
 * via the provider CLI (Bob), and writes the same JSON augmented with
 * block_diagram / call_edges / diagram_error per symbol.
 *
 * See docs/zdoc-ai-mode.md.
 */
#include <ctype.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bob_client.h"
#include "closure.h"
#include "json.h"
#include "util.h"

#define VERSION "0.1.0"
#define DEFAULT_TMPL "bob explain --diagram --brief --lang {lang}"

typedef struct {
    zj_val *symbol;      /* symbol object to augment */
    const char *module;  /* file path, for logging */
    const char *name;
    char *snippet;       /* malloc'd */
    const char *lang;    /* display name for {lang} */
    bc_lang blang;
    bc_result res;
    int done_ok;
} job;

typedef struct {
    job *jobs;
    size_t njobs;
    size_t next;         /* guarded by mu */
    pthread_mutex_t mu;
    const bc_cfg *cfg;
} pool;

static struct {
    long generated, cached, repaired, failed, calls_made;
} stats;
static pthread_mutex_t stats_mu = PTHREAD_MUTEX_INITIALIZER;

static void *worker(void *arg)
{
    pool *P = (pool *)arg;
    for (;;) {
        pthread_mutex_lock(&P->mu);
        size_t i = P->next < P->njobs ? P->next++ : (size_t)-1;
        pthread_mutex_unlock(&P->mu);
        if (i == (size_t)-1)
            return NULL;
        job *J = &P->jobs[i];
        int rc = bc_generate(P->cfg, J->lang, J->snippet, &J->res);
        J->done_ok = (rc == 0);
        pthread_mutex_lock(&stats_mu);
        stats.calls_made += J->res.attempts;
        if (rc == 0) {
            if (J->res.cached)
                stats.cached++;
            else
                stats.generated++;
            if (J->res.repaired)
                stats.repaired++;
        } else {
            stats.failed++;
        }
        pthread_mutex_unlock(&stats_mu);
    }
}

static int is_exec_kind(const char *k)
{
    return k && (!strcmp(k, "function") || !strcmp(k, "procedure") ||
                 !strcmp(k, "entry"));
}

/* case-insensitive strcmp for case-folding languages */
static int name_eq(const char *a, const char *b, int fold)
{
    if (!fold)
        return strcmp(a, b) == 0;
    for (; *a && *b; a++, b++)
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b))
            return 0;
    return *a == *b;
}

/* Match call-node texts against extracted symbol names of the module. */
static void attach_call_edges(zj_doc *doc, zj_val *symbol, bc_result *res,
                              zj_val *symbols, int fold)
{
    if (!res->ncalls)
        return;
    zj_val *arr = zj_new(doc, ZJ_ARR);
    for (size_t c = 0; c < res->ncalls; c++) {
        /* tokenize the call text; any token matching a symbol name links */
        const char *t = res->calls[c];
        const char *p = t;
        while (*p) {
            if (!isalnum((unsigned char)*p) && *p != '_' && *p != '$' &&
                *p != '#' && *p != '@') {
                p++;
                continue;
            }
            const char *s = p;
            while (isalnum((unsigned char)*p) || *p == '_' || *p == '$' ||
                   *p == '#' || *p == '@')
                p++;
            char word[128];
            size_t wl = (size_t)(p - s);
            if (wl == 0 || wl >= sizeof word)
                continue;
            memcpy(word, s, wl);
            word[wl] = 0;
            for (zj_val *m = symbols->child; m; m = m->next) {
                const char *nm = zj_str(zj_get(m, "name"), NULL);
                if (!nm || m == symbol)
                    continue;
                if (name_eq(word, nm, fold)) {
                    /* dedupe */
                    int dup = 0;
                    for (zj_val *e = arr->child; e; e = e->next)
                        if (e->str && strcmp(e->str, nm) == 0)
                            dup = 1;
                    if (!dup)
                        zj_push(arr, zj_new_str(doc, nm));
                    break;
                }
            }
        }
    }
    if (arr->child)
        zj_set(doc, symbol, "call_edges", arr);
}

static void log_failure(FILE *fl, const char *module, const char *name,
                        const bc_result *res)
{
    if (!fl)
        return;
    fprintf(fl, "module=%s symbol=%s error=%s\n", module, name,
            res->errmsg ? res->errmsg : "unknown");
    if (res->raw && *res->raw) {
        fputs("--- raw response (truncated) ---\n", fl);
        fputs(res->raw, fl);
        fputs("\n--- end ---\n", fl);
    }
}

static void usage(void)
{
    fprintf(stderr,
        "usage: zdoc-bob-client [options] < parser.json > augmented.json\n"
        "  --input <file>        read JSON from file instead of stdin\n"
        "  --bob-cli <path>      provider binary (default: bob)\n"
        "  --bob-args \"<args>\"   extra provider arguments\n"
        "  --ai-command \"<tmpl>\" full command template ({lang} substituted)\n"
        "  --ai-jobs <n>         concurrent provider calls (default 4)\n"
        "  --ai-timeout <secs>   per-call timeout (default 60)\n"
        "  --ai-cache-dir <dir>  cache directory (default ~/.cache/zdoc)\n"
        "  --ai-no-cache         disable the diagram cache\n"
        "  --ai-refresh          ignore cached entries, regenerate\n"
        "  --ai-record <dir>     record request/response pairs\n"
        "  --fail-log <path>     failure log (default ./zdoc-ai-failures.log)\n"
        "  --max-context <n>     closure character budget (default 4000)\n"
        "  --depth <n>           transitive closure depth (default 1)\n"
        "  --version, --help\n");
}

int main(int argc, char **argv)
{
    signal(SIGPIPE, SIG_IGN);

    const char *input_path = NULL;
    const char *bob_cli = "bob";
    const char *bob_args = NULL;
    const char *ai_command = NULL;
    const char *fail_log_path = "zdoc-ai-failures.log";
    bc_cfg cfg = {0};
    cfg.timeout_sec = 60;
    int jobs = 4;
    size_t max_context = 4000;
    int depth = 1;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        const char *v = (i + 1 < argc) ? argv[i + 1] : NULL;
        if (!strcmp(a, "--version")) {
            puts("zdoc-bob-client " VERSION " (skill " BC_SKILL_VERSION ")");
            return 0;
        } else if (!strcmp(a, "--help")) {
            usage();
            return 0;
        } else if (!strcmp(a, "--input") && v) {
            input_path = v; i++;
        } else if (!strcmp(a, "--bob-cli") && v) {
            bob_cli = v; i++;
        } else if (!strcmp(a, "--bob-args") && v) {
            bob_args = v; i++;
        } else if (!strcmp(a, "--ai-command") && v) {
            ai_command = v; i++;
        } else if (!strcmp(a, "--ai-jobs") && v) {
            jobs = atoi(v); i++;
        } else if (!strcmp(a, "--ai-timeout") && v) {
            cfg.timeout_sec = atoi(v); i++;
        } else if (!strcmp(a, "--ai-cache-dir") && v) {
            cfg.cache_dir = v; i++;
        } else if (!strcmp(a, "--ai-no-cache")) {
            cfg.no_cache = 1;
        } else if (!strcmp(a, "--ai-refresh")) {
            cfg.refresh = 1;
        } else if (!strcmp(a, "--ai-record") && v) {
            cfg.record_dir = v; i++;
        } else if (!strcmp(a, "--fail-log") && v) {
            fail_log_path = v; i++;
        } else if (!strcmp(a, "--max-context") && v) {
            max_context = (size_t)atol(v); i++;
        } else if (!strcmp(a, "--depth") && v) {
            depth = atoi(v); i++;
        } else {
            fprintf(stderr, "zdoc-bob-client: unknown option %s\n", a);
            usage();
            return 2;
        }
    }
    if (jobs < 1)
        jobs = 1;
    if (jobs > 64)
        jobs = 64;

    /* provider command template */
    bc_sb tmpl = {0};
    if (ai_command) {
        bc_sb_adds(&tmpl, ai_command);
    } else {
        bc_sb_adds(&tmpl, bob_cli);
        bc_sb_adds(&tmpl, " explain --diagram --brief --lang {lang}");
        if (bob_args && *bob_args) {
            bc_sb_addc(&tmpl, ' ');
            bc_sb_adds(&tmpl, bob_args);
        }
    }
    char *tmpl_s = bc_sb_take(&tmpl);
    cfg.command_tmpl = tmpl_s;

    /* read input */
    size_t in_n = 0;
    char *in = input_path ? bc_read_file(input_path, &in_n)
                          : bc_read_fd(0, &in_n);
    if (!in) {
        fprintf(stderr, "zdoc-bob-client: cannot read input\n");
        return 1;
    }
    zj_doc *doc = zj_parse(in, in_n);
    free(in);
    if (!doc || doc->err) {
        fprintf(stderr, "zdoc-bob-client: input is not valid JSON (%s)\n",
                doc && doc->err ? doc->err : "oom");
        zj_doc_free(doc);
        return 1;
    }

    zj_val *modules = zj_get(doc->root, "modules");
    job *jobsv = NULL;
    size_t njobs = 0, jcap = 0;

    for (zj_val *mod = modules ? modules->child : NULL; mod; mod = mod->next) {
        const char *file = zj_str(zj_get(mod, "file"), "?");
        bc_lang blang = bc_lang_parse(zj_str(zj_get(mod, "language"), "c"));
        zj_val *symbols = zj_get(mod, "symbols");
        if (!symbols)
            continue;

        /* declarations for the closure index */
        bc_arena dA = {0};
        zj_val *jdecls = zj_get(mod, "declarations");
        size_t ndecls = zj_len(jdecls);
        bc_decl *decls = NULL;
        if (ndecls) {
            decls = (bc_decl *)bc_alloc(&dA, ndecls * sizeof *decls);
            size_t di = 0;
            for (zj_val *d = jdecls->child; d; d = d->next, di++) {
                zj_val *names = zj_get(d, "names");
                size_t nn = zj_len(names);
                const char **nv =
                    (const char **)bc_alloc(&dA, (nn ? nn : 1) * sizeof *nv);
                size_t ni = 0;
                for (zj_val *nm = names ? names->child : NULL; nm; nm = nm->next)
                    if (nm->t == ZJ_STR)
                        nv[ni++] = nm->str;
                decls[di].names = nv;
                decls[di].nnames = ni;
                decls[di].text = zj_str(zj_get(d, "text"), "");
                decls[di].line = (long)zj_num(zj_get(d, "line"), 0);
            }
        }
        bc_index *idx = bc_index_build(decls, ndecls, blang);

        for (zj_val *sym = symbols->child; sym; sym = sym->next) {
            const char *kind = zj_str(zj_get(sym, "kind"), NULL);
            if (!is_exec_kind(kind))
                continue;
            const char *name = zj_str(zj_get(sym, "name"), "?");
            const char *body = zj_str(zj_get(sym, "body"), NULL);
            if (!body || !*body) {
                /* no --ai-context data: mark and continue */
                zj_set(doc, sym, "diagram_error", zj_new_bool(doc, 1));
                pthread_mutex_lock(&stats_mu);
                stats.failed++;
                pthread_mutex_unlock(&stats_mu);
                continue;
            }
            /* closure */
            size_t nclo = 0;
            const bc_decl **clo =
                bc_closure(body, idx, blang, max_context, depth, &nclo);

            /* callee lines: referenced names that are extracted symbols */
            size_t nrefs = 0;
            char **refs = bc_extract_refs(body, blang, &nrefs);
            const char *callee_lines[8];
            char *callee_owned[8];
            size_t ncallees = 0;
            int fold = bc_lang_folds_case(blang);
            for (size_t r = 0; r < nrefs && ncallees < 8; r++) {
                for (zj_val *m = symbols->child; m; m = m->next) {
                    if (m == sym)
                        continue;
                    const char *mk = zj_str(zj_get(m, "kind"), NULL);
                    if (!mk || (!is_exec_kind(mk) && strcmp(mk, "prototype")))
                        continue;
                    const char *mn = zj_str(zj_get(m, "name"), NULL);
                    if (!mn || !name_eq(refs[r], mn, fold))
                        continue;
                    const char *sig = zj_str(zj_get(m, "signature"), mn);
                    const char *brief =
                        zj_str(zj_get(zj_get(m, "doc"), "brief"), NULL);
                    bc_sb line = {0};
                    bc_sb_adds(&line, mn);
                    bc_sb_adds(&line, ": ");
                    bc_sb_adds(&line, sig);
                    if (brief) {
                        bc_sb_adds(&line, " — ");
                        bc_sb_adds(&line, brief);
                    }
                    callee_owned[ncallees] = bc_sb_take(&line);
                    callee_lines[ncallees] = callee_owned[ncallees];
                    ncallees++;
                    break;
                }
            }
            bc_refs_free(refs, nrefs);

            const char *brief =
                zj_str(zj_get(zj_get(sym, "doc"), "brief"), NULL);
            char *snippet = bc_build_snippet(brief, clo, nclo, callee_lines,
                                             ncallees, blang, body);
            free((void *)clo);
            for (size_t c = 0; c < ncallees; c++)
                free(callee_owned[c]);

            if (njobs == jcap) {
                jcap = jcap ? jcap * 2 : 64;
                jobsv = (job *)realloc(jobsv, jcap * sizeof *jobsv);
            }
            memset(&jobsv[njobs], 0, sizeof jobsv[njobs]);
            jobsv[njobs].symbol = sym;
            jobsv[njobs].module = file;
            jobsv[njobs].name = name;
            jobsv[njobs].snippet = snippet;
            jobsv[njobs].lang = bc_lang_display(blang);
            jobsv[njobs].blang = blang;
            njobs++;
        }
        bc_index_free(idx);
        bc_arena_free(&dA);
    }

    /* run the pool */
    pool P = {jobsv, njobs, 0, PTHREAD_MUTEX_INITIALIZER, &cfg};
    int nthreads = jobs < (int)njobs ? jobs : (int)njobs;
    if (nthreads < 1)
        nthreads = 1;
    pthread_t tids[64];
    for (int t = 0; t < nthreads; t++)
        pthread_create(&tids[t], NULL, worker, &P);
    for (int t = 0; t < nthreads; t++)
        pthread_join(tids[t], NULL);

    /* attach results in deterministic (source) order */
    FILE *fl = NULL;
    for (size_t i = 0; i < njobs; i++) {
        job *J = &jobsv[i];
        zj_val *mod_syms = NULL;
        /* find the symbols array containing this symbol's module — the
         * module pointer chain isn't kept; refetch via modules loop */
        for (zj_val *mod = modules ? modules->child : NULL; mod;
             mod = mod->next) {
            const char *file = zj_str(zj_get(mod, "file"), "?");
            if (file == J->module || strcmp(file, J->module) == 0) {
                mod_syms = zj_get(mod, "symbols");
                break;
            }
        }
        if (J->done_ok) {
            zj_set(doc, J->symbol, "block_diagram",
                   zj_new_str(doc, J->res.mermaid));
            if (mod_syms)
                attach_call_edges(doc, J->symbol, &J->res, mod_syms,
                                  bc_lang_folds_case(J->blang));
        } else {
            zj_set(doc, J->symbol, "diagram_error", zj_new_bool(doc, 1));
            if (!fl)
                fl = fopen(fail_log_path, "a");
            log_failure(fl, J->module, J->name, &J->res);
        }
        bc_result_free(&J->res);
        free(J->snippet);
    }
    if (fl)
        fclose(fl);
    free(jobsv);

    zj_write(stdout, doc->root);
    fputc('\n', stdout);
    fprintf(stderr,
            "zdoc-bob-client: generated %ld, cached %ld, repaired %ld, "
            "failed %ld (provider calls: %ld)\n",
            stats.generated, stats.cached, stats.repaired, stats.failed,
            stats.calls_made);

    zj_doc_free(doc);
    free(tmpl_s);
    return stats.failed && !stats.generated && !stats.cached ? 1 : 0;
}
