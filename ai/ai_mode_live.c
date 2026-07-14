/* ai_mode_live.c — manual end-to-end check of the whole AI-mode interface
 * (zdoc_ai_annotate) against the *real* Bob CLI.
 *
 * Not part of any CI target. Requires bob on PATH with a valid session/API key
 * and the zdoc-diagram extension linked. Build & run:
 *
 *     make -C ai live
 *
 * It stands in for the daemon: it writes a small two-symbol PL/X fixture to
 * disk, builds the module_tree tables and a Module the way the parser+daemon
 * would (pathIndex stamped, symbol line numbers set), then runs the online pass
 * and prints the Mermaid diagram Bob returned for each symbol. Exit 0 iff every
 * symbol came back annotated. The real parser is deliberately not involved —
 * driving it is the daemon's job (see the PR "For the daemon" section); this
 * proves the interface end to end with real Bob.
 */
#include "ai_mode.h"
#include "../extractor/doc_extractor/module_tree/fs_walk.h"
#include "../extractor/doc_extractor/module_tree/modtree_tables.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A two-procedure PL/X fixture. Symbol line numbers below index into this. */
static const char *FIXTURE =
    "INITPROC: PROC(ANCHOR) RETURNS(FIXED BIN(31));\n"  /* line 1  */
    "    CBSTG = OBTAIN(LENGTH(CB));\n"                   /* line 2  */
    "    IF CBSTG = 0 THEN\n"                             /* line 3  */
    "        RETURN(8);\n"                                /* line 4  */
    "    CBPTR = CBSTG;\n"                                /* line 5  */
    "    CBPTR->CBNEXT = ANCHOR->ANCHFRST;\n"             /* line 6  */
    "    ANCHOR->ANCHFRST = CBPTR;\n"                     /* line 7  */
    "    RETURN(0);\n"                                    /* line 8  */
    "END INITPROC;\n"                                     /* line 9  */
    "\n"                                                  /* line 10 */
    "TERMPROC: PROC(ANCHOR) RETURNS(FIXED BIN(31));\n"    /* line 11 */
    "    IF ANCHOR->ANCHFRST = NULL THEN\n"               /* line 12 */
    "        RETURN(4);\n"                                /* line 13 */
    "    CALL RELEASE(ANCHOR->ANCHFRST);\n"               /* line 14 */
    "    ANCHOR->ANCHFRST = NULL;\n"                      /* line 15 */
    "    RETURN(0);\n"                                    /* line 16 */
    "END TERMPROC;\n";                                    /* line 17 */

int main(void)
{
    const char *dir = "/tmp/zdoc_ai_live";
    const char *file = "INITPROC.plx";
    char full[512];
    snprintf(full, sizeof full, "%s/%s", dir, file);

    if (system("mkdir -p /tmp/zdoc_ai_live") != 0) {
        fprintf(stderr, "could not create fixture dir\n");
        return 2;
    }
    FILE *f = fopen(full, "w");
    if (!f) {
        fprintf(stderr, "could not write fixture %s\n", full);
        return 2;
    }
    fputs(FIXTURE, f);
    fclose(f);

    /* resolve_source_path() glues fs_walk_root_prefix onto the reconstructed
     * relative path, exactly as it will in the daemon. Set the prefix to the
     * fixture dir's parent so "<prefix>/zdoc_ai_live/INITPROC.plx" opens. */
    strncpy(fs_walk_root_prefix, "/tmp", FS_WALK_PATH_MAX - 1);
    fs_walk_root_prefix[FS_WALK_PATH_MAX - 1] = '\0';

    modtree_dir_table_t dirs;
    modtree_file_table_t files;
    modtree_dir_table_init(&dirs);
    modtree_file_table_init(&files);
    int d = modtree_intern_dir(&dirs, "zdoc_ai_live", -1);
    int fi = modtree_intern_file(&files, file, d);

    /* Build the Module the parser+daemon would hand us. */
    Symbol syms[2];
    memset(syms, 0, sizeof syms);
    syms[0].name = "INITPROC";
    syms[0].description = "Initialise subsystem";
    syms[0].line = 1;
    syms[1].name = "TERMPROC";
    syms[1].description = "Terminate subsystem";
    syms[1].line = 11;

    Module mod;
    memset(&mod, 0, sizeof mod);
    mod.filename = (char *)file;
    mod.symbols = syms;
    mod.symbolCount = 2;
    mod.pathIndex = fi;

    AiOptions opt;
    opt.bob = bob_config_default(); /* bob on PATH, no extra args */

    int n = zdoc_ai_annotate(&dirs, &files, &mod, 1, &opt);
    printf("zdoc_ai_annotate: %d of %d symbols annotated\n", n, mod.symbolCount);

    for (int i = 0; i < mod.symbolCount; i++) {
        printf("\n==== %s ====\n%s\n", syms[i].name,
               syms[i].diagram ? syms[i].diagram
                               : "(NULL — bob missing, non-zero exit, or no "
                                 "flowchart in response)");
        free(syms[i].diagram);
    }

    modtree_dir_table_free(&dirs);
    modtree_file_table_free(&files);
    return (n == mod.symbolCount) ? 0 : 1;
}
