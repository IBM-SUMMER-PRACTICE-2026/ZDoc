/* Table-driven tests for the graph core (docs/zdoc-ai-mode.md §The graph
 * contract): JSON parse, validation, normalization/repair, mermaid
 * fallback, serialization, call harvesting. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "graph.h"

static int failures;

#define CHECK(cond, msg)                                                    \
    do {                                                                    \
        if (!(cond)) {                                                      \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg);   \
            failures++;                                                     \
        }                                                                   \
    } while (0)

static const char valid_json[] =
    "{\"nodes\":["
    "{\"id\":\"A\",\"kind\":\"step\",\"text\":\"Entry: f\"},"
    "{\"id\":\"B\",\"kind\":\"decision\",\"text\":\"ok?\"},"
    "{\"id\":\"C\",\"kind\":\"return\",\"text\":\"Return RC=8\"},"
    "{\"id\":\"D\",\"kind\":\"call\",\"text\":\"Call TERMPROC\"},"
    "{\"id\":\"E\",\"kind\":\"return\",\"text\":\"Return RC=0\"}],"
    "\"edges\":["
    "{\"from\":\"A\",\"to\":\"B\"},"
    "{\"from\":\"B\",\"to\":\"C\",\"label\":\"No\"},"
    "{\"from\":\"B\",\"to\":\"D\",\"label\":\"Yes\"},"
    "{\"from\":\"D\",\"to\":\"E\"}]}";

static bg_graph *parse(const char *s, char *err, int *rep)
{
    char ebuf[256] = "";
    int r = 0;
    bg_graph *g = bg_parse(s, strlen(s), ebuf, sizeof ebuf, &r);
    if (err)
        strcpy(err, ebuf);
    if (rep)
        *rep = r;
    return g;
}

static void test_valid(void)
{
    int rep = -1;
    bg_graph *g = parse(valid_json, NULL, &rep);
    CHECK(g != NULL, "valid graph parses");
    if (!g)
        return;
    CHECK(rep == 0, "clean JSON is not marked repaired");
    CHECK(g->nn == 5 && g->ne == 4, "node/edge counts");
    char *mm = bg_to_mermaid(g);
    CHECK(strncmp(mm, "flowchart TD", 12) == 0, "mermaid header");
    CHECK(strstr(mm, "A[\"Entry: f\"]") != NULL, "step node rendered");
    CHECK(strstr(mm, "B{\"ok?\"}") != NULL, "decision node rendered");
    CHECK(strstr(mm, "D(\"Call TERMPROC\")") != NULL, "call node rendered");
    CHECK(strstr(mm, "B -- No --> C") != NULL, "labeled edge rendered");
    free(mm);

    size_t nc = 0;
    const char **calls = bg_calls(g, &nc);
    CHECK(nc == 1 && strcmp(calls[0], "Call TERMPROC") == 0, "call harvested");
    free((void *)calls);

    char *canon = bg_canonical_json(g);
    /* canonical json re-parses to an identical graph */
    bg_graph *g2 = parse(canon, NULL, NULL);
    CHECK(g2 && g2->nn == g->nn && g2->ne == g->ne, "canonical roundtrip");
    free(canon);
    bg_graph_free(g2);
    bg_graph_free(g);
}

static void test_repair(void)
{
    /* prose + fence around valid JSON: repaired, not rejected */
    char buf[2048];
    snprintf(buf, sizeof buf,
             "Sure! Here is the diagram you asked for:\n```json\n%s\n```\n"
             "Let me know if you need anything else.",
             valid_json);
    int rep = 0;
    bg_graph *g = parse(buf, NULL, &rep);
    CHECK(g != NULL, "noisy but valid response accepted");
    CHECK(rep == 1, "noise marked as repaired");
    bg_graph_free(g);
}

static void test_mermaid_fallback(void)
{
    const char mm[] =
        "```mermaid\n"
        "flowchart TD\n"
        "    A[Entry: GETWORK] --> B{GETMAIN successful?}\n"
        "    %% a comment to strip\n"
        "    B -- No --> C[Return RC=8]\n"
        "    B -- Yes --> D[Store pointer]\n"
        "    D --> E(Call FREEMAIN)\n"
        "    E --> F[Return RC=0]\n"
        "```\n";
    int rep = 0;
    bg_graph *g = parse(mm, NULL, &rep);
    CHECK(g != NULL, "mermaid block fallback accepted");
    if (!g)
        return;
    CHECK(rep == 1, "mermaid fallback counts as repaired");
    CHECK(g->nn == 6, "six nodes parsed");
    int found_ret = 0, found_call = 0, found_dec = 0;
    for (size_t i = 0; i < g->nn; i++) {
        if (!strcmp(g->nodes[i].kind, "return")) found_ret++;
        if (!strcmp(g->nodes[i].kind, "call")) found_call++;
        if (!strcmp(g->nodes[i].kind, "decision")) found_dec++;
    }
    CHECK(found_dec == 1, "decision node inferred from braces");
    CHECK(found_call == 1, "call node inferred from parens");
    CHECK(found_ret == 2, "Return-prefixed steps become return nodes");
    bg_graph_free(g);
}

static void test_rejections(void)
{
    static const struct {
        const char *desc;
        const char *input;
    } cases[] = {
        {"empty response", ""},
        {"pure prose", "I cannot generate a diagram for this snippet."},
        {"no nodes array", "{\"edges\":[]}"},
        {"zero nodes", "{\"nodes\":[],\"edges\":[]}"},
        {"bad kind",
         "{\"nodes\":[{\"id\":\"A\",\"kind\":\"box\",\"text\":\"x\"}],"
         "\"edges\":[]}"},
        {"missing text",
         "{\"nodes\":[{\"id\":\"A\",\"kind\":\"step\"}],\"edges\":[]}"},
        {"duplicate ids",
         "{\"nodes\":[{\"id\":\"A\",\"kind\":\"step\",\"text\":\"x\"},"
         "{\"id\":\"A\",\"kind\":\"step\",\"text\":\"y\"}],\"edges\":[]}"},
        {"edge to unknown node",
         "{\"nodes\":[{\"id\":\"A\",\"kind\":\"step\",\"text\":\"x\"}],"
         "\"edges\":[{\"from\":\"A\",\"to\":\"Z\"}]}"},
        {"unreachable node",
         "{\"nodes\":[{\"id\":\"A\",\"kind\":\"step\",\"text\":\"x\"},"
         "{\"id\":\"B\",\"kind\":\"step\",\"text\":\"y\"}],\"edges\":[]}"},
        {"unlabeled decision edge",
         "{\"nodes\":[{\"id\":\"A\",\"kind\":\"decision\",\"text\":\"x?\"},"
         "{\"id\":\"B\",\"kind\":\"step\",\"text\":\"y\"}],"
         "\"edges\":[{\"from\":\"A\",\"to\":\"B\"}]}"},
        {"graph LR instead of flowchart", "graph LR\n A --> B\n"},
        {"subgraph forbidden",
         "flowchart TD\n subgraph S\n A[x]\n end\n"},
    };
    for (size_t i = 0; i < sizeof cases / sizeof *cases; i++) {
        char err[256] = "";
        bg_graph *g = parse(cases[i].input, err, NULL);
        if (g) {
            fprintf(stderr, "FAIL: expected rejection: %s\n", cases[i].desc);
            failures++;
            bg_graph_free(g);
        }
    }

    /* > 14 nodes rejected */
    bc_sb sb = {0};
    bc_sb_adds(&sb, "{\"nodes\":[");
    for (int i = 0; i < 15; i++) {
        char nb[96];
        snprintf(nb, sizeof nb,
                 "%s{\"id\":\"N%d\",\"kind\":\"step\",\"text\":\"s\"}",
                 i ? "," : "", i);
        bc_sb_adds(&sb, nb);
    }
    bc_sb_adds(&sb, "],\"edges\":[");
    for (int i = 0; i < 14; i++) {
        char eb[64];
        snprintf(eb, sizeof eb, "%s{\"from\":\"N%d\",\"to\":\"N%d\"}",
                 i ? "," : "", i, i + 1);
        bc_sb_adds(&sb, eb);
    }
    bc_sb_adds(&sb, "]}");
    char *big = bc_sb_take(&sb);
    bg_graph *g = parse(big, NULL, NULL);
    CHECK(g == NULL, "more than 14 nodes rejected");
    if (g)
        bg_graph_free(g);
    free(big);
}

static void test_escaping(void)
{
    const char tricky[] =
        "{\"nodes\":[{\"id\":\"A\",\"kind\":\"step\","
        "\"text\":\"He said \\\"hi\\\" [sic]\"}],\"edges\":[]}";
    bg_graph *g = parse(tricky, NULL, NULL);
    CHECK(g != NULL, "tricky text accepted (escaping is our job)");
    if (!g)
        return;
    char *mm = bg_to_mermaid(g);
    CHECK(strstr(mm, "#quot;hi#quot;") != NULL, "quotes escaped for mermaid");
    CHECK(strstr(mm, "\"He said") != NULL, "label is quoted");
    free(mm);
    bg_graph_free(g);
}

int main(void)
{
    test_valid();
    test_repair();
    test_mermaid_fallback();
    test_rejections();
    test_escaping();
    if (failures) {
        fprintf(stderr, "test_graph: %d FAILURE(S)\n", failures);
        return 1;
    }
    puts("test_graph: all tests passed");
    return 0;
}
