/* live_bob.c — manual end-to-end check against the *real* Bob CLI.
 *
 * Not part of `make test` (that stays offline). Requires bob on PATH with a
 * valid session/API key, and the zdoc-diagram skill discoverable. Build & run:
 *
 *     make live      # from ai/bob_client
 *
 * Feeds the PL/X golden example through the full path: context closure ->
 * snippet -> Bob -> sanitized flowchart. Prints both the snippet sent and the
 * diagram returned. Exit 0 iff a diagram came back. */
#include "bob_client.h"
#include "closure.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    static const char *cb_names[]   = {"CB", "CBEYE", "CBFLAGS", "CBNEXT",
                                       "CBPTR"};
    static const char *init_names[] = {"CBINIT"};
    static const char *stg_names[]  = {"CBSTG"};
    static const char *anch_names[] = {"ANCH", "ANCHEYE", "ANCHFRST",
                                       "ANCHOR"};
    static const char *unused_names[] = {"UNUSED"};

    bc_decl decls[] = {
        {cb_names, 5,
         "DCL 1 CB BASED(CBPTR),\n"
         "      2 CBEYE   CHAR(4),\n"
         "      2 CBFLAGS BIT(8),\n"
         "      2 CBNEXT  PTR;", 1},
        {init_names, 1, "DCL CBINIT BIT(8) CONSTANT('80'X);", 6},
        {stg_names, 1, "DCL CBSTG FIXED BIN(31);", 7},
        {anch_names, 4,
         "DCL 1 ANCH BASED(ANCHOR),\n"
         "      2 ANCHEYE  CHAR(4),\n"
         "      2 ANCHFRST PTR;", 8},
        {unused_names, 1, "DCL UNUSED CHAR(80);", 11}, /* must NOT be selected */
    };

    const char *body =
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

    bc_lang lang = BC_LANG_PLX;

    bc_index *idx = bc_index_build(decls, 5, lang);
    size_t nc = 0;
    const bc_decl **c = bc_closure(body, idx, lang, 4000, 1, &nc);
    char *snippet = bc_build_snippet("Initialise subsystem", c, nc, NULL, 0,
                                     lang, body);

    printf("==== snippet sent to bob ====\n%s\n", snippet);

    BobConfig cfg = bob_config_default(); /* cli="bob" on PATH */
    char *diagram = bob_diagram(&cfg, bc_lang_display(lang), snippet);

    printf("==== diagram from bob ====\n%s\n",
           diagram ? diagram : "(NULL - bob missing, non-zero exit, or no "
                               "flowchart in response)");

    int ok = diagram != NULL;
    free(diagram);
    free(snippet);
    free((void *)c);
    bc_index_free(idx);
    return ok ? 0 : 1;
}
