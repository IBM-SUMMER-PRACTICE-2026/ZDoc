#include "bob_client.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "graph.h"
#include "sha256.h"
#include "util.h"

extern char **environ;

#define BC_RAW_LOG_MAX 500

static const char *const retry_suffix =
    "\n\nYour previous response violated the output contract. Return ONLY "
    "one JSON object with nodes and edges per the zdoc-diagram skill. "
    "No prose.";

/* ------------------------------------------------------------- helpers */

char *bc_cache_dir(const bc_cfg *cfg)
{
    bc_sb sb = {0};
    if (cfg->cache_dir && *cfg->cache_dir) {
        bc_sb_adds(&sb, cfg->cache_dir);
    } else {
        const char *home = getenv("HOME");
        bc_sb_adds(&sb, home && *home ? home : ".");
        bc_sb_adds(&sb, "/.cache/zdoc");
    }
    return bc_sb_take(&sb);
}

static int mkpath(const char *path)
{
    char buf[1024];
    size_t n = strlen(path);
    if (n >= sizeof buf)
        return -1;
    memcpy(buf, path, n + 1);
    for (char *p = buf + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(buf, 0755) != 0 && errno != EEXIST)
                return -1;
            *p = '/';
        }
    }
    if (mkdir(buf, 0755) != 0 && errno != EEXIST)
        return -1;
    return 0;
}

/* Split a command template on whitespace, substituting {lang}.
 * Returns a NULL-terminated argv; free with free_argv. No shell quoting —
 * the snippet travels on stdin, never in argv. */
static char **build_argv(const char *tmpl, const char *lang)
{
    size_t cap = 16, n = 0;
    char **av = (char **)malloc(cap * sizeof *av);
    if (!av)
        return NULL;
    const char *p = tmpl;
    while (*p) {
        while (*p == ' ' || *p == '\t')
            p++;
        if (!*p)
            break;
        const char *s = p;
        while (*p && *p != ' ' && *p != '\t')
            p++;
        size_t len = (size_t)(p - s);
        /* substitute {lang} inside the word */
        bc_sb w = {0};
        for (size_t i = 0; i < len; i++) {
            if (i + 6 <= len && memcmp(s + i, "{lang}", 6) == 0) {
                bc_sb_adds(&w, lang);
                i += 5;
            } else {
                bc_sb_addc(&w, s[i]);
            }
        }
        if (n + 2 > cap) {
            cap *= 2;
            char **nv = (char **)realloc(av, cap * sizeof *av);
            if (!nv) {
                bc_sb_reset(&w);
                break;
            }
            av = nv;
        }
        av[n++] = bc_sb_take(&w);
    }
    av[n] = NULL;
    return av;
}

static void free_argv(char **av)
{
    if (!av)
        return;
    for (size_t i = 0; av[i]; i++)
        free(av[i]);
    free(av);
}

/* ---------------------------------------------------------------- spawn */

/* Serializes pipe creation + spawn across worker threads. Without this —
 * and without FD_CLOEXEC below — a concurrently spawned provider inherits
 * copies of a sibling's pipe ends, its stdin never reaches EOF, and the
 * siblings deadlock on each other until the timeout kills them. */
static pthread_mutex_t spawn_mu = PTHREAD_MUTEX_INITIALIZER;

/* Run argv, feed `input` on stdin, capture stdout. Returns 0 with *out set,
 * -1 on spawn error, -2 on timeout, -3 on nonzero exit. */
static int run_provider(char **argv, const char *input, int timeout_sec,
                        char **out)
{
    int in_pipe[2], out_pipe[2];
    *out = NULL;
    pthread_mutex_lock(&spawn_mu);
    if (pipe(in_pipe) != 0) {
        pthread_mutex_unlock(&spawn_mu);
        return -1;
    }
    if (pipe(out_pipe) != 0) {
        close(in_pipe[0]);
        close(in_pipe[1]);
        pthread_mutex_unlock(&spawn_mu);
        return -1;
    }
    /* keep our pipe ends out of every other child; the adddup2 below
     * clears CLOEXEC on this child's own stdio copies */
    for (int i = 0; i < 2; i++) {
        fcntl(in_pipe[i], F_SETFD, FD_CLOEXEC);
        fcntl(out_pipe[i], F_SETFD, FD_CLOEXEC);
    }

    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_adddup2(&fa, in_pipe[0], 0);
    posix_spawn_file_actions_adddup2(&fa, out_pipe[1], 1);
    posix_spawn_file_actions_addopen(&fa, 2, "/dev/null", O_WRONLY, 0);

    pid_t pid;
    int rc = posix_spawnp(&pid, argv[0], &fa, NULL, argv, environ);
    posix_spawn_file_actions_destroy(&fa);
    pthread_mutex_unlock(&spawn_mu);
    close(in_pipe[0]);
    close(out_pipe[1]);
    if (rc != 0) {
        close(in_pipe[1]);
        close(out_pipe[0]);
        return -1;
    }

    /* write the snippet (SIGPIPE ignored process-wide in main) */
    size_t off = 0, ilen = strlen(input);
    while (off < ilen) {
        ssize_t w = write(in_pipe[1], input + off, ilen - off);
        if (w < 0) {
            if (errno == EINTR)
                continue;
            break; /* EPIPE: child closed stdin early — fine */
        }
        off += (size_t)w;
    }
    close(in_pipe[1]);

    /* read stdout under a deadline */
    bc_sb sb = {0};
    int timed_out = 0;
    long remaining_ms = (long)timeout_sec * 1000;
    for (;;) {
        struct pollfd pf = {out_pipe[0], POLLIN, 0};
        int pr = poll(&pf, 1, remaining_ms > 250 ? 250 : (int)remaining_ms);
        if (pr > 0) {
            char buf[16384];
            ssize_t r = read(out_pipe[0], buf, sizeof buf);
            if (r > 0) {
                bc_sb_add(&sb, buf, (size_t)r);
                continue;
            }
            if (r == 0)
                break; /* EOF */
            if (errno == EINTR)
                continue;
            break;
        }
        remaining_ms -= 250;
        if (remaining_ms <= 0) {
            timed_out = 1;
            break;
        }
    }
    close(out_pipe[0]);

    if (timed_out) {
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        bc_sb_reset(&sb);
        return -2;
    }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
        ;
    char *text = bc_sb_take(&sb);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        free(text);
        return -3;
    }
    *out = text;
    return 0;
}

/* ---------------------------------------------------------------- cache */

static char *cache_path(const bc_cfg *cfg, const char *key_hex)
{
    char *dir = bc_cache_dir(cfg);
    bc_sb sb = {0};
    bc_sb_adds(&sb, dir);
    bc_sb_addc(&sb, '/');
    bc_sb_adds(&sb, key_hex);
    bc_sb_adds(&sb, ".json");
    free(dir);
    return bc_sb_take(&sb);
}

static void cache_key(const bc_cfg *cfg, const char *snippet, char hex[65])
{
    bc_sha256 c;
    uint8_t d[32];
    bc_sha256_init(&c);
    bc_sha256_update(&c, snippet, strlen(snippet));
    bc_sha256_update(&c, "\0", 1);
    bc_sha256_update(&c, BC_SKILL_VERSION, strlen(BC_SKILL_VERSION));
    bc_sha256_update(&c, "\0", 1);
    bc_sha256_update(&c, cfg->command_tmpl, strlen(cfg->command_tmpl));
    bc_sha256_final(&c, d);
    static const char hd[] = "0123456789abcdef";
    for (int i = 0; i < 32; i++) {
        hex[i * 2] = hd[d[i] >> 4];
        hex[i * 2 + 1] = hd[d[i] & 15];
    }
    hex[64] = 0;
}

static void cache_store(const bc_cfg *cfg, const char *key_hex,
                        const char *canonical)
{
    char *dir = bc_cache_dir(cfg);
    if (mkpath(dir) != 0) {
        free(dir);
        return;
    }
    free(dir);
    char *path = cache_path(cfg, key_hex);
    bc_sb tmp = {0};
    bc_sb_adds(&tmp, path);
    bc_sb_adds(&tmp, ".tmp.");
    char pidbuf[32];
    snprintf(pidbuf, sizeof pidbuf, "%ld", (long)getpid());
    bc_sb_adds(&tmp, pidbuf);
    char *tmppath = bc_sb_take(&tmp);
    FILE *f = fopen(tmppath, "w");
    if (f) {
        fputs(canonical, f);
        fclose(f);
        rename(tmppath, path); /* atomic publish */
    }
    free(tmppath);
    free(path);
}

/* ---------------------------------------------------------------- record */

static void record_pair(const bc_cfg *cfg, const char *key_hex,
                        const char *req, const char *res)
{
    if (!cfg->record_dir)
        return;
    if (mkpath(cfg->record_dir) != 0)
        return;
    bc_sb sb = {0};
    bc_sb_adds(&sb, cfg->record_dir);
    bc_sb_addc(&sb, '/');
    bc_sb_adds(&sb, key_hex);
    bc_sb_adds(&sb, ".req.txt");
    char *p = bc_sb_take(&sb);
    FILE *f = fopen(p, "w");
    if (f) {
        fputs(req, f);
        fclose(f);
    }
    free(p);
    bc_sb_adds(&sb, cfg->record_dir);
    bc_sb_addc(&sb, '/');
    bc_sb_adds(&sb, key_hex);
    bc_sb_adds(&sb, ".res.txt");
    p = bc_sb_take(&sb);
    f = fopen(p, "w");
    if (f) {
        fputs(res ? res : "", f);
        fclose(f);
    }
    free(p);
}

/* -------------------------------------------------------------- generate */

static void fill_from_graph(bc_result *out, bg_graph *g, int repaired)
{
    out->mermaid = bg_to_mermaid(g);
    out->repaired = repaired;
    size_t nc = 0;
    const char **calls = bg_calls(g, &nc);
    if (nc) {
        out->calls = (char **)malloc(nc * sizeof *out->calls);
        if (out->calls) {
            for (size_t i = 0; i < nc; i++)
                out->calls[i] = strdup(calls[i]);
            out->ncalls = nc;
        }
    }
    free((void *)calls);
}

int bc_generate(const bc_cfg *cfg, const char *lang_name,
                const char *snippet, bc_result *out)
{
    memset(out, 0, sizeof *out);
    char key[65];
    cache_key(cfg, snippet, key);

    /* cache read */
    if (!cfg->no_cache && !cfg->refresh) {
        char *path = cache_path(cfg, key);
        size_t n = 0;
        char *data = bc_read_file(path, &n);
        free(path);
        if (data) {
            char err[128] = "";
            int rep = 0;
            bg_graph *g = bg_parse(data, n, err, sizeof err, &rep);
            free(data);
            if (g) {
                fill_from_graph(out, g, 0);
                out->cached = 1;
                bg_graph_free(g);
                return 0;
            }
            /* corrupt cache entry: fall through and regenerate */
        }
    }

    char **argv = build_argv(cfg->command_tmpl, lang_name);
    if (!argv || !argv[0]) {
        free_argv(argv);
        out->errmsg = strdup("empty provider command");
        return -1;
    }

    const char *last_err = "unknown error";
    for (int attempt = 1; attempt <= 2; attempt++) {
        const char *req = snippet;
        char *req_owned = NULL;
        if (attempt == 2) {
            bc_sb sb = {0};
            bc_sb_adds(&sb, snippet);
            bc_sb_adds(&sb, retry_suffix);
            req_owned = bc_sb_take(&sb);
            req = req_owned;
        }
        char *resp = NULL;
        int rc = run_provider(argv, req, cfg->timeout_sec > 0 ? cfg->timeout_sec : 60,
                              &resp);
        out->attempts = attempt;
        record_pair(cfg, key, req, resp);
        free(req_owned);
        if (rc == -1) {
            last_err = "failed to spawn provider";
        } else if (rc == -2) {
            last_err = "provider timed out";
        } else if (rc == -3) {
            last_err = "provider exited nonzero";
        } else {
            char err[128] = "";
            int rep = 0;
            bg_graph *g = bg_parse(resp, strlen(resp), err, sizeof err, &rep);
            if (g) {
                fill_from_graph(out, g, rep);
                if (!cfg->no_cache) {
                    char *canon = bg_canonical_json(g);
                    if (canon) {
                        cache_store(cfg, key, canon);
                        free(canon);
                    }
                }
                bg_graph_free(g);
                free(resp);
                free_argv(argv);
                return 0;
            }
            last_err = *err ? err : "invalid response";
            /* keep a truncated copy of the last raw response for logging */
            free(out->raw);
            size_t rl = strlen(resp);
            if (rl > BC_RAW_LOG_MAX)
                rl = BC_RAW_LOG_MAX;
            out->raw = (char *)malloc(rl + 1);
            if (out->raw) {
                memcpy(out->raw, resp, rl);
                out->raw[rl] = 0;
            }
            free(resp);
            /* validation errors persist only until this scope; dup now */
            free(out->errmsg);
            out->errmsg = strdup(last_err);
            continue;
        }
        free(resp);
        free(out->errmsg);
        out->errmsg = strdup(last_err);
    }
    free_argv(argv);
    if (!out->errmsg)
        out->errmsg = strdup(last_err);
    return -1;
}

void bc_result_free(bc_result *r)
{
    if (!r)
        return;
    free(r->mermaid);
    for (size_t i = 0; i < r->ncalls; i++)
        free(r->calls[i]);
    free(r->calls);
    free(r->errmsg);
    free(r->raw);
    memset(r, 0, sizeof *r);
}
