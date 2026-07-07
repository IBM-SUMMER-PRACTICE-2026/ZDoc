#include "graph.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json.h"

#define BG_MAX_NODES 14

static void seterr(char *err, size_t errsz, const char *msg)
{
    if (err && errsz) {
        strncpy(err, msg, errsz - 1);
        err[errsz - 1] = 0;
    }
}

/* ------------------------------------------------------- normalization */

/* Find the span of the first balanced top-level JSON object, honoring
 * strings. Returns 0 when none found. */
static int find_json_span(const char *s, size_t n, size_t *a, size_t *b)
{
    size_t i = 0;
    while (i < n && s[i] != '{')
        i++;
    if (i == n)
        return 0;
    size_t start = i;
    int depth = 0, in_str = 0;
    for (; i < n; i++) {
        char c = s[i];
        if (in_str) {
            if (c == '\\')
                i++;
            else if (c == '"')
                in_str = 0;
            continue;
        }
        if (c == '"')
            in_str = 1;
        else if (c == '{')
            depth++;
        else if (c == '}') {
            depth--;
            if (depth == 0) {
                *a = start;
                *b = i + 1;
                return 1;
            }
        }
    }
    return 0;
}

/* Find a mermaid block: fenced ```mermaid ... ``` or a bare region starting
 * at a "flowchart TD" line. Returns malloc'd copy without fences. */
static char *find_mermaid_block(const char *s, size_t n)
{
    const char *fence = NULL;
    for (size_t i = 0; i + 10 <= n; i++) {
        if (memcmp(s + i, "```mermaid", 10) == 0) {
            fence = s + i + 10;
            break;
        }
    }
    if (fence) {
        const char *body = fence;
        while (body < s + n && *body != '\n')
            body++;
        if (body < s + n)
            body++;
        const char *end = body;
        while (end + 3 <= s + n && memcmp(end, "```", 3) != 0)
            end++;
        if (end + 3 > s + n)
            return NULL;
        size_t len = (size_t)(end - body);
        char *r = (char *)malloc(len + 1);
        if (!r)
            return NULL;
        memcpy(r, body, len);
        r[len] = 0;
        return r;
    }
    /* bare flowchart TD */
    for (size_t i = 0; i + 12 <= n; i++) {
        if (memcmp(s + i, "flowchart TD", 12) == 0 &&
            (i == 0 || s[i - 1] == '\n' || isspace((unsigned char)s[i - 1]))) {
            size_t len = n - i;
            char *r = (char *)malloc(len + 1);
            if (!r)
                return NULL;
            memcpy(r, s + i, len);
            r[len] = 0;
            return r;
        }
    }
    return NULL;
}

/* ---------------------------------------------------------- construction */

static bg_graph *bg_new(void)
{
    return (bg_graph *)calloc(1, sizeof(bg_graph));
}

static const char *g_dup(bg_graph *g, const char *s, size_t n)
{
    return bc_adup(&g->A, s, n);
}

typedef struct {
    bg_node *v;
    size_t n, cap;
} nvec;
typedef struct {
    bg_edge *v;
    size_t n, cap;
} evec;

static void npush(nvec *d, bg_node x)
{
    if (d->n == d->cap) {
        d->cap = d->cap ? d->cap * 2 : 16;
        d->v = (bg_node *)realloc(d->v, d->cap * sizeof *d->v);
    }
    if (d->v)
        d->v[d->n++] = x;
}

static void epush(evec *d, bg_edge x)
{
    if (d->n == d->cap) {
        d->cap = d->cap ? d->cap * 2 : 16;
        d->v = (bg_edge *)realloc(d->v, d->cap * sizeof *d->v);
    }
    if (d->v)
        d->v[d->n++] = x;
}

/* ------------------------------------------------------------ JSON path */

static int kind_ok(const char *k)
{
    return k && (!strcmp(k, "step") || !strcmp(k, "decision") ||
                 !strcmp(k, "call") || !strcmp(k, "return"));
}

static bg_graph *graph_from_json(const char *s, size_t n, char *err,
                                 size_t errsz)
{
    zj_doc *d = zj_parse(s, n);
    if (!d)
        return NULL;
    if (d->err) {
        seterr(err, errsz, "response is not valid JSON");
        zj_doc_free(d);
        return NULL;
    }
    zj_val *nodes = zj_get(d->root, "nodes");
    zj_val *edges = zj_get(d->root, "edges");
    if (!nodes || nodes->t != ZJ_ARR) {
        seterr(err, errsz, "JSON has no nodes array");
        zj_doc_free(d);
        return NULL;
    }
    bg_graph *g = bg_new();
    if (!g) {
        zj_doc_free(d);
        return NULL;
    }
    nvec nv = {0};
    evec ev = {0};
    for (zj_val *m = nodes->child; m; m = m->next) {
        const char *id = zj_str(zj_get(m, "id"), NULL);
        const char *kind = zj_str(zj_get(m, "kind"), NULL);
        const char *text = zj_str(zj_get(m, "text"), NULL);
        bg_node nd = {
            id ? g_dup(g, id, strlen(id)) : NULL,
            kind ? g_dup(g, kind, strlen(kind)) : NULL,
            text ? g_dup(g, text, strlen(text)) : NULL,
        };
        npush(&nv, nd);
    }
    if (edges && edges->t == ZJ_ARR) {
        for (zj_val *m = edges->child; m; m = m->next) {
            const char *from = zj_str(zj_get(m, "from"), NULL);
            const char *to = zj_str(zj_get(m, "to"), NULL);
            const char *label = zj_str(zj_get(m, "label"), NULL);
            bg_edge e = {
                from ? g_dup(g, from, strlen(from)) : NULL,
                to ? g_dup(g, to, strlen(to)) : NULL,
                label ? g_dup(g, label, strlen(label)) : NULL,
            };
            epush(&ev, e);
        }
    }
    zj_doc_free(d);
    g->nodes = nv.v;
    g->nn = nv.n;
    g->edges = ev.v;
    g->ne = ev.n;
    return g;
}

/* --------------------------------------------------------- mermaid path */

/* Parse a node reference: ID optionally followed by a shape+text:
 * A, A[Text], A["Text"], A{Text}, A(Text). Registers the node. */
static const char *parse_node_ref(bg_graph *g, nvec *nv, const char *p,
                                  const char **out_id)
{
    while (*p == ' ' || *p == '\t')
        p++;
    const char *ids = p;
    while (isalnum((unsigned char)*p) || *p == '_')
        p++;
    if (p == ids)
        return NULL;
    const char *id = g_dup(g, ids, (size_t)(p - ids));
    *out_id = id;

    char open = *p;
    const char *kind = NULL;
    char close = 0;
    if (open == '[') {
        kind = "step";
        close = ']';
    } else if (open == '{') {
        kind = "decision";
        close = '}';
    } else if (open == '(') {
        kind = "call";
        close = ')';
    }
    const char *text = NULL;
    if (kind) {
        p++;
        int quoted = (*p == '"');
        if (quoted)
            p++;
        const char *ts = p;
        while (*p && *p != close && !(quoted && *p == '"'))
            p++;
        const char *te = p;
        if (quoted && *p == '"')
            p++;
        if (*p == close)
            p++;
        while (te > ts && isspace((unsigned char)te[-1]))
            te--;
        text = g_dup(g, ts, (size_t)(te - ts));
        if (strncmp(text, "Return", 6) == 0 && strcmp(kind, "step") == 0)
            kind = "return";
    }
    /* register (first definition with text wins) */
    for (size_t i = 0; i < nv->n; i++) {
        if (strcmp(nv->v[i].id, id) == 0) {
            if (!nv->v[i].text && text) {
                nv->v[i].text = text;
                nv->v[i].kind = kind;
            }
            return p;
        }
    }
    bg_node nd = {id, kind ? kind : "step", text};
    npush(nv, nd);
    return p;
}

static bg_graph *graph_from_mermaid(const char *block, char *err,
                                    size_t errsz)
{
    bg_graph *g = bg_new();
    if (!g)
        return NULL;
    nvec nv = {0};
    evec ev = {0};
    const char *p = block;
    int saw_header = 0, bad = 0;

    while (*p && !bad) {
        /* one line */
        const char *ls = p;
        while (*p && *p != '\n')
            p++;
        const char *le = p;
        if (*p)
            p++;
        while (ls < le && isspace((unsigned char)*ls))
            ls++;
        while (le > ls && isspace((unsigned char)le[-1]))
            le--;
        if (ls == le)
            continue;
        size_t llen = (size_t)(le - ls);
        if (llen >= 2 && ls[0] == '%' && ls[1] == '%')
            continue; /* comment: tolerated on input, stripped */
        if (!saw_header) {
            if (llen >= 12 && memcmp(ls, "flowchart TD", 12) == 0) {
                saw_header = 1;
                continue;
            }
            seterr(err, errsz, "mermaid block does not start with flowchart TD");
            bad = 1;
            break;
        }
        if (llen >= 8 && memcmp(ls, "subgraph", 8) == 0) {
            seterr(err, errsz, "subgraph is not allowed");
            bad = 1;
            break;
        }
        /* line = noderef ( edge noderef )* */
        char line[1024];
        if (llen >= sizeof line) {
            seterr(err, errsz, "mermaid line too long");
            bad = 1;
            break;
        }
        memcpy(line, ls, llen);
        line[llen] = 0;
        const char *q = line;
        const char *prev_id = NULL;
        q = parse_node_ref(g, &nv, q, &prev_id);
        if (!q) {
            seterr(err, errsz, "cannot parse mermaid node");
            bad = 1;
            break;
        }
        for (;;) {
            while (*q == ' ' || *q == '\t')
                q++;
            if (!*q)
                break;
            /* edge: --> or -- label --> */
            if (q[0] != '-' || q[1] != '-') {
                seterr(err, errsz, "cannot parse mermaid edge");
                bad = 1;
                break;
            }
            q += 2;
            const char *label = NULL;
            if (*q != '>') {
                const char *lstart = q;
                const char *arrow = strstr(q, "-->");
                if (!arrow) {
                    seterr(err, errsz, "unterminated mermaid edge label");
                    bad = 1;
                    break;
                }
                const char *lend = arrow;
                while (lstart < lend && isspace((unsigned char)*lstart))
                    lstart++;
                while (lend > lstart && isspace((unsigned char)lend[-1]))
                    lend--;
                label = g_dup(g, lstart, (size_t)(lend - lstart));
                q = arrow + 3;
            } else {
                q++; /* consumed "-->" minus the "--" */
            }
            const char *to_id = NULL;
            q = parse_node_ref(g, &nv, q, &to_id);
            if (!q) {
                seterr(err, errsz, "cannot parse mermaid edge target");
                bad = 1;
                break;
            }
            bg_edge e = {prev_id, to_id, label};
            epush(&ev, e);
            prev_id = to_id;
        }
    }
    if (!saw_header && !bad) {
        seterr(err, errsz, "no flowchart TD header");
        bad = 1;
    }
    if (bad) {
        free(nv.v);
        free(ev.v);
        bg_graph_free(g);
        return NULL;
    }
    g->nodes = nv.v;
    g->nn = nv.n;
    g->edges = ev.v;
    g->ne = ev.n;
    return g;
}

/* ------------------------------------------------------------ validation */

static const bg_node *find_node(const bg_graph *g, const char *id)
{
    for (size_t i = 0; i < g->nn; i++)
        if (g->nodes[i].id && strcmp(g->nodes[i].id, id) == 0)
            return &g->nodes[i];
    return NULL;
}

static int bg_validate(const bg_graph *g, char *err, size_t errsz)
{
    if (g->nn == 0) {
        seterr(err, errsz, "graph has no nodes");
        return 0;
    }
    if (g->nn > BG_MAX_NODES) {
        seterr(err, errsz, "graph exceeds 14 nodes");
        return 0;
    }
    for (size_t i = 0; i < g->nn; i++) {
        const bg_node *nd = &g->nodes[i];
        if (!nd->id || !*nd->id) {
            seterr(err, errsz, "node missing id");
            return 0;
        }
        if (!kind_ok(nd->kind)) {
            seterr(err, errsz, "node has invalid kind");
            return 0;
        }
        if (!nd->text || !*nd->text) {
            seterr(err, errsz, "node missing text");
            return 0;
        }
        for (size_t k = 0; k < i; k++) {
            if (strcmp(g->nodes[k].id, nd->id) == 0) {
                seterr(err, errsz, "duplicate node id");
                return 0;
            }
        }
    }
    for (size_t i = 0; i < g->ne; i++) {
        const bg_edge *e = &g->edges[i];
        if (!e->from || !e->to || !find_node(g, e->from) ||
            !find_node(g, e->to)) {
            seterr(err, errsz, "edge references unknown node");
            return 0;
        }
        const bg_node *src = find_node(g, e->from);
        if (!strcmp(src->kind, "decision") && (!e->label || !*e->label)) {
            seterr(err, errsz, "decision out-edge missing label");
            return 0;
        }
    }
    /* reachability from the first node */
    int reach[BG_MAX_NODES] = {0};
    size_t stack[BG_MAX_NODES], sp = 0;
    reach[0] = 1;
    stack[sp++] = 0;
    while (sp) {
        size_t cur = stack[--sp];
        for (size_t i = 0; i < g->ne; i++) {
            if (strcmp(g->edges[i].from, g->nodes[cur].id) != 0)
                continue;
            for (size_t k = 0; k < g->nn; k++) {
                if (!reach[k] && strcmp(g->nodes[k].id, g->edges[i].to) == 0) {
                    reach[k] = 1;
                    stack[sp++] = k;
                }
            }
        }
    }
    for (size_t i = 0; i < g->nn; i++) {
        if (!reach[i]) {
            seterr(err, errsz, "node unreachable from entry");
            return 0;
        }
    }
    return 1;
}

/* --------------------------------------------------------------- public */

bg_graph *bg_parse(const char *raw, size_t n, char *err, size_t errsz,
                   int *repaired)
{
    if (repaired)
        *repaired = 0;
    if (!raw || !n) {
        seterr(err, errsz, "empty response");
        return NULL;
    }
    /* try JSON first */
    size_t a = 0, b = 0;
    if (find_json_span(raw, n, &a, &b)) {
        /* noise = anything outside the object other than whitespace or a
         * single ```json fence */
        int noisy = 0;
        for (size_t i = 0; i < a; i++)
            if (!isspace((unsigned char)raw[i]))
                noisy = 1;
        for (size_t i = b; i < n; i++)
            if (!isspace((unsigned char)raw[i]))
                noisy = 1;
        char jerrbuf[128] = "";
        bg_graph *g = graph_from_json(raw + a, b - a, jerrbuf, sizeof jerrbuf);
        if (g) {
            if (bg_validate(g, err, errsz)) {
                if (repaired && noisy)
                    *repaired = 1;
                return g;
            }
            bg_graph_free(g);
            return NULL;
        }
    }
    /* fallback: mermaid block */
    char *block = find_mermaid_block(raw, n);
    if (block) {
        bg_graph *g = graph_from_mermaid(block, err, errsz);
        free(block);
        if (g) {
            if (bg_validate(g, err, errsz)) {
                if (repaired)
                    *repaired = 1; /* non-primary contract counts as repair */
                return g;
            }
            bg_graph_free(g);
            return NULL;
        }
        return NULL;
    }
    if (err && !*err)
        seterr(err, errsz, "no JSON graph or mermaid block in response");
    return NULL;
}

static void mm_text(bc_sb *sb, const char *t)
{
    /* quoted label: escape double quotes as #quot;, drop control chars */
    bc_sb_addc(sb, '"');
    for (const unsigned char *p = (const unsigned char *)t; *p; p++) {
        if (*p == '"')
            bc_sb_adds(sb, "#quot;");
        else if (*p >= 0x20 || *p == '\t')
            bc_sb_addc(sb, (char)*p);
        else
            bc_sb_addc(sb, ' ');
    }
    bc_sb_addc(sb, '"');
}

char *bg_to_mermaid(const bg_graph *g)
{
    bc_sb sb = {0};
    bc_sb_adds(&sb, "flowchart TD");
    for (size_t i = 0; i < g->nn; i++) {
        const bg_node *nd = &g->nodes[i];
        bc_sb_adds(&sb, "\n    ");
        bc_sb_adds(&sb, nd->id);
        const char *open = "[", *close = "]";
        if (!strcmp(nd->kind, "decision")) {
            open = "{";
            close = "}";
        } else if (!strcmp(nd->kind, "call")) {
            open = "(";
            close = ")";
        }
        bc_sb_adds(&sb, open);
        mm_text(&sb, nd->text);
        bc_sb_adds(&sb, close);
    }
    for (size_t i = 0; i < g->ne; i++) {
        const bg_edge *e = &g->edges[i];
        bc_sb_adds(&sb, "\n    ");
        bc_sb_adds(&sb, e->from);
        if (e->label && *e->label) {
            bc_sb_adds(&sb, " -- ");
            /* labels are plain words per the skill; strip risky chars */
            for (const char *p = e->label; *p; p++)
                if ((unsigned char)*p >= 0x20 && *p != '-' && *p != '>')
                    bc_sb_addc(&sb, *p);
            bc_sb_adds(&sb, " -->");
        } else {
            bc_sb_adds(&sb, " -->");
        }
        bc_sb_addc(&sb, ' ');
        bc_sb_adds(&sb, e->to);
    }
    bc_sb_addc(&sb, '\n');
    return bc_sb_take(&sb);
}

static void js_str(bc_sb *sb, const char *s)
{
    bc_sb_addc(sb, '"');
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p == '"' || *p == '\\') {
            bc_sb_addc(sb, '\\');
            bc_sb_addc(sb, (char)*p);
        } else if (*p == '\n') {
            bc_sb_adds(sb, "\\n");
        } else if (*p < 0x20) {
            char buf[8];
            snprintf(buf, sizeof buf, "\\u%04x", *p);
            bc_sb_adds(sb, buf);
        } else {
            bc_sb_addc(sb, (char)*p);
        }
    }
    bc_sb_addc(sb, '"');
}

char *bg_canonical_json(const bg_graph *g)
{
    bc_sb sb = {0};
    bc_sb_adds(&sb, "{\"nodes\":[");
    for (size_t i = 0; i < g->nn; i++) {
        if (i)
            bc_sb_addc(&sb, ',');
        bc_sb_adds(&sb, "{\"id\":");
        js_str(&sb, g->nodes[i].id);
        bc_sb_adds(&sb, ",\"kind\":");
        js_str(&sb, g->nodes[i].kind);
        bc_sb_adds(&sb, ",\"text\":");
        js_str(&sb, g->nodes[i].text);
        bc_sb_addc(&sb, '}');
    }
    bc_sb_adds(&sb, "],\"edges\":[");
    for (size_t i = 0; i < g->ne; i++) {
        if (i)
            bc_sb_addc(&sb, ',');
        bc_sb_adds(&sb, "{\"from\":");
        js_str(&sb, g->edges[i].from);
        bc_sb_adds(&sb, ",\"to\":");
        js_str(&sb, g->edges[i].to);
        if (g->edges[i].label && *g->edges[i].label) {
            bc_sb_adds(&sb, ",\"label\":");
            js_str(&sb, g->edges[i].label);
        }
        bc_sb_addc(&sb, '}');
    }
    bc_sb_adds(&sb, "]}");
    return bc_sb_take(&sb);
}

const char **bg_calls(const bg_graph *g, size_t *out_n)
{
    size_t n = 0;
    for (size_t i = 0; i < g->nn; i++)
        if (!strcmp(g->nodes[i].kind, "call"))
            n++;
    *out_n = n;
    if (!n)
        return NULL;
    const char **v = (const char **)malloc(n * sizeof *v);
    if (!v) {
        *out_n = 0;
        return NULL;
    }
    size_t o = 0;
    for (size_t i = 0; i < g->nn; i++)
        if (!strcmp(g->nodes[i].kind, "call"))
            v[o++] = g->nodes[i].text;
    return v;
}

void bg_graph_free(bg_graph *g)
{
    if (!g)
        return;
    free(g->nodes);
    free(g->edges);
    bc_arena_free(&g->A);
    free(g);
}
