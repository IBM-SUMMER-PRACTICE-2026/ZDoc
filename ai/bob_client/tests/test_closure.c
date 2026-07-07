/* Unit tests for closure assembly (docs/zdoc-ai-mode.md §Closure assembly).
 * Fixture mirrors the PL/X golden example from the zdoc-diagram skill. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "closure.h"

static int failures;

#define CHECK(cond, msg)                                                    \
    do {                                                                    \
        if (!(cond)) {                                                      \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg);   \
            failures++;                                                     \
        }                                                                   \
    } while (0)

static const char *cb_names[] = {"CB", "CBEYE", "CBFLAGS", "CBNEXT",
                                       "CBPTR"};
static const char *cbinit_names[] = {"CBINIT"};
static const char *cbstg_names[] = {"CBSTG"};
static const char *anch_names[] = {"ANCH", "ANCHEYE", "ANCHFRST",
                                         "ANCHOR"};
static const char *unused_names[] = {"UNUSED"};

static const char cb_text[] =
    "DCL 1 CB BASED(CBPTR),\n"
    "      2 CBEYE   CHAR(4),\n"
    "      2 CBFLAGS BIT(8),\n"
    "      2 CBNEXT  PTR;";
static const char cbinit_text[] = "DCL CBINIT BIT(8) CONSTANT('80'X);";
static const char cbstg_text[] = "DCL CBSTG FIXED BIN(31);";
static const char anch_text[] =
    "DCL 1 ANCH BASED(ANCHOR),\n"
    "      2 ANCHEYE  CHAR(4),\n"
    "      2 ANCHFRST PTR;";
static const char unused_text[] = "DCL UNUSED CHAR(80);";

static const char body[] =
    "INITPROC: PROC(ANCHOR) RETURNS(FIXED BIN(31));\n"
    "    CBSTG = OBTAIN(LENGTH(CB));\n"
    "    IF CBSTG = 0 THEN\n"
    "        RETURN(8);\n"
    "    CBPTR = CBSTG;\n"
    "    CBPTR->CBEYE = 'ZDCB';\n"
    "    CBPTR->CBFLAGS = CBINIT;\n"
    "    CBPTR->CBNEXT = ANCHOR->ANCHFRST;\n"
    "    ANCHOR->ANCHFRST = CBPTR;\n"
    "    RETURN(0);\n"
    "END INITPROC;";

static bc_decl make_decls[5];

static void setup(void)
{
    make_decls[0] = (bc_decl){cb_names, 5, cb_text, 1};
    make_decls[1] = (bc_decl){cbinit_names, 1, cbinit_text, 6};
    make_decls[2] = (bc_decl){cbstg_names, 1, cbstg_text, 7};
    make_decls[3] = (bc_decl){anch_names, 4, anch_text, 8};
    make_decls[4] = (bc_decl){unused_names, 1, unused_text, 11};
}

static int closure_has(const bc_decl **c, size_t n, const char *text)
{
    for (size_t i = 0; i < n; i++)
        if (strcmp(c[i]->text, text) == 0)
            return 1;
    return 0;
}

static void test_plx_closure(void)
{
    bc_index *ix = bc_index_build(make_decls, 5, BC_LANG_PLX);
    size_t n = 0;
    const bc_decl **c = bc_closure(body, ix, BC_LANG_PLX, 4000, 1, &n);

    CHECK(closure_has(c, n, cb_text), "CB block included");
    CHECK(closure_has(c, n, cbinit_text), "CBINIT included");
    CHECK(closure_has(c, n, cbstg_text), "CBSTG included");
    CHECK(closure_has(c, n, anch_text), "ANCH block included");
    CHECK(!closure_has(c, n, unused_text), "UNUSED excluded");

    /* multi-name DCL block appears exactly once despite 5 member refs */
    int cb_count = 0;
    for (size_t i = 0; i < n; i++)
        if (strcmp(c[i]->text, cb_text) == 0)
            cb_count++;
    CHECK(cb_count == 1, "multi-name declaration deduped");

    free((void *)c);
    bc_index_free(ix);
}

static void test_case_folding(void)
{
    bc_index *ix = bc_index_build(make_decls, 5, BC_LANG_PLX);
    /* PL/X lookups are case-insensitive */
    CHECK(bc_index_lookup(ix, "cbflags") != NULL, "PL/X lower-case lookup");
    CHECK(bc_index_lookup(ix, "CbFlAgS") != NULL, "PL/X mixed-case lookup");
    /* the BASED pointer resolves to the structure declaration */
    const bc_decl *d = bc_index_lookup(ix, "CBPTR");
    CHECK(d && strcmp(d->text, cb_text) == 0, "BASED pointer indexed");
    bc_index_free(ix);

    /* C is case-sensitive */
    static const char *cn[] = {"node_t"};
    bc_decl cd[] = {{cn, 1, "typedef struct node { int x; } node_t;", 1}};
    bc_index *cx = bc_index_build(cd, 1, BC_LANG_C);
    CHECK(bc_index_lookup(cx, "node_t") != NULL, "C exact lookup");
    CHECK(bc_index_lookup(cx, "NODE_T") == NULL, "C case-sensitive miss");
    bc_index_free(cx);
}

static void test_budget(void)
{
    /* tier-1 (transitive) dropped before tier-0 under a tight budget */
    static const char *t0n[] = {"USESTYPE"};
    static const char *t1n[] = {"BASETYPE"};
    /* tier-0 decl references BASETYPE, seeding tier 1 */
    bc_decl d[] = {
        {t0n, 1, "DCL USESTYPE TYPE(BASETYPE);", 1},
        {t1n, 1, "DCL BASETYPE FIXED BIN(31);", 2},
    };
    bc_index *ix = bc_index_build(d, 2, BC_LANG_PLX);
    const char *b = "P: PROC; X = USESTYPE; END P;";

    size_t n = 0;
    const bc_decl **c = bc_closure(b, ix, BC_LANG_PLX, 4000, 1, &n);
    CHECK(n == 2, "roomy budget keeps both tiers");
    CHECK(c && c[0] == &d[0], "tier-0 first");
    free((void *)c);

    /* budget fits only tier 0 */
    size_t tight = strlen(d[0].text) + 2;
    c = bc_closure(b, ix, BC_LANG_PLX, tight, 1, &n);
    CHECK(n == 1, "tight budget drops transitive declaration");
    CHECK(c && c[0] == &d[0], "tier-0 survives the tight budget");
    free((void *)c);

    /* budget smaller than the first declaration: still sent */
    c = bc_closure(b, ix, BC_LANG_PLX, 3, 1, &n);
    CHECK(n == 1, "never zero context when context exists");
    free((void *)c);
    bc_index_free(ix);
}

static void test_snippet(void)
{
    bc_index *ix = bc_index_build(make_decls, 5, BC_LANG_PLX);
    size_t n = 0;
    const bc_decl **c = bc_closure(body, ix, BC_LANG_PLX, 4000, 1, &n);
    char *s = bc_build_snippet("Initialise subsystem", c, n, NULL, 0,
                               BC_LANG_PLX, body);
    CHECK(strstr(s, "DOC:\nInitialise subsystem") == s, "DOC section first");
    CHECK(strstr(s, "DECLARATIONS:\n") != NULL, "DECLARATIONS present");
    CHECK(strstr(s, "FUNCTION (PL/X):\n") != NULL, "FUNCTION header");
    CHECK(strstr(s, "FUNCTION (PL/X):") > strstr(s, "DECLARATIONS:"),
          "FUNCTION comes last");
    free(s);
    free((void *)c);

    /* empty closure => no DECLARATIONS header at all */
    char *s2 = bc_build_snippet(NULL, NULL, 0, NULL, 0, BC_LANG_C,
                                "int f(void) { return 0; }");
    CHECK(strstr(s2, "DECLARATIONS:") == NULL, "no DECLARATIONS when empty");
    CHECK(strstr(s2, "DOC:") == NULL, "no DOC when empty");
    CHECK(strncmp(s2, "FUNCTION (C):\n", 14) == 0, "snippet is body only");
    free(s2);
    bc_index_free(ix);
}

static void test_refs(void)
{
    size_t n = 0;
    char **r = bc_extract_refs("if (foo) { bar_baz(QUX); }", BC_LANG_C, &n);
    int has_foo = 0, has_bar = 0, has_if = 0;
    for (size_t i = 0; i < n; i++) {
        if (!strcmp(r[i], "foo")) has_foo = 1;
        if (!strcmp(r[i], "bar_baz")) has_bar = 1;
        if (!strcmp(r[i], "if")) has_if = 1;
    }
    CHECK(has_foo && has_bar, "identifiers collected");
    CHECK(!has_if, "keywords excluded");
    /* sorted + unique */
    for (size_t i = 1; i < n; i++)
        CHECK(strcmp(r[i - 1], r[i]) < 0, "refs sorted unique");
    bc_refs_free(r, n);
}

int main(void)
{
    setup();
    test_plx_closure();
    test_case_folding();
    test_budget();
    test_snippet();
    test_refs();
    if (failures) {
        fprintf(stderr, "test_closure: %d FAILURE(S)\n", failures);
        return 1;
    }
    puts("test_closure: all tests passed");
    return 0;
}
