/* ai_mode_live.c — manual end-to-end check of the whole AI-mode interface
 * (zdoc_ai_annotate) against the *real* Bob CLI, across languages.
 *
 * Not part of any CI target. Requires bob on PATH with a valid session/API key
 * and the zdoc-diagram extension linked. Build & run:
 *
 *     make -C ai live                 # built-in PL/X + C + Java fixtures
 *     ./ai_mode_live path/to/file.c   # diagram one real file (whole-file)
 *
 * With no argument it writes a small multi-symbol fixture per language, builds
 * the module_tree tables and the Modules the parser+daemon would produce
 * (pathIndex stamped, symbol line numbers set), runs the online pass, and prints
 * the Mermaid diagram Bob returned for each symbol. The language for each file
 * is derived from its extension by the interface itself — the same mapping the
 * daemon will use.
 *
 * With a file argument it treats the whole file as a single symbol at line 1 —
 * a quick way to point online mode at your own .plx/.c/.java source without the
 * parser (which is the daemon's job) supplying per-symbol line numbers.
 *
 * Exit 0 iff every symbol came back annotated.
 */
#include "ai_mode.h"
#include "../extractor/doc_extractor/module_tree/fs_walk.h"
#include "../extractor/doc_extractor/module_tree/modtree_tables.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ----- built-in fixtures, one per language ----- */

static const char *PLX_SRC =
    "INITPROC: PROC(ANCHOR) RETURNS(FIXED BIN(31));\n"  /* line 1  */
    "    CBSTG = OBTAIN(LENGTH(CB));\n"                   /* 2  */
    "    IF CBSTG = 0 THEN\n"                             /* 3  */
    "        RETURN(8);\n"                                /* 4  */
    "    CBPTR = CBSTG;\n"                                /* 5  */
    "    ANCHOR->ANCHFRST = CBPTR;\n"                     /* 6  */
    "    RETURN(0);\n"                                    /* 7  */
    "END INITPROC;\n"                                     /* 8  */
    "\n"                                                  /* 9  */
    "TERMPROC: PROC(ANCHOR) RETURNS(FIXED BIN(31));\n"    /* 10 */
    "    IF ANCHOR->ANCHFRST = NULL THEN\n"               /* 11 */
    "        RETURN(4);\n"                                /* 12 */
    "    CALL RELEASE(ANCHOR->ANCHFRST);\n"               /* 13 */
    "    RETURN(0);\n"                                    /* 14 */
    "END TERMPROC;\n";                                    /* 15 */

static const char *C_SRC =
    "int classify(int n) {\n"          /* line 1 */
    "    if (n < 0) return -1;\n"       /* 2 */
    "    if (n == 0) return 0;\n"       /* 3 */
    "    return 1;\n"                   /* 4 */
    "}\n"                               /* 5 */
    "\n"                                /* 6 */
    "int sum_to(int n) {\n"            /* line 7 */
    "    int s = 0;\n"                  /* 8 */
    "    for (int i = 1; i <= n; i++)\n"/* 9 */
    "        s += i;\n"                 /* 10 */
    "    return s;\n"                   /* 11 */
    "}\n";                              /* 12 */

static const char *JAVA_SRC =
    "class Demo {\n"                              /* line 1 */
    "    int classify(int n) {\n"                 /* line 2 */
    "        if (n < 0) return -1;\n"             /* 3 */
    "        return n == 0 ? 0 : 1;\n"            /* 4 */
    "    }\n"                                     /* 5 */
    "    int sumTo(int n) {\n"                    /* line 6 */
    "        int s = 0;\n"                        /* 7 */
    "        for (int i = 1; i <= n; i++) s += i;\n" /* 8 */
    "        return s;\n"                         /* 9 */
    "    }\n"                                     /* 10 */
    "}\n";                                        /* 11 */

/* Write `text` to <dir>/<name>, return 0 on success. */
static int write_fixture(const char *dir, const char *name, const char *text)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "w");
    if (!f)
        return -1;
    fputs(text, f);
    fclose(f);
    return 0;
}

static int run_and_report(const modtree_dir_table_t *dirs,
                          const modtree_file_table_t *files, Module *modules,
                          size_t nmod)
{
    AiOptions opt;
    opt.bob = bob_config_default(); /* bob on PATH, no extra args */

    int annotated = zdoc_ai_annotate(dirs, files, modules, nmod, &opt);

    int total = 0;
    for (size_t m = 0; m < nmod; m++) {
        printf("\n######## %s ########\n",
               modules[m].filename ? modules[m].filename : "(file)");
        for (int i = 0; i < modules[m].symbolCount; i++) {
            Symbol *s = &modules[m].symbols[i];
            total++;
            printf("\n==== %s ====\n%s\n", s->name ? s->name : "(symbol)",
                   s->diagram ? s->diagram
                              : "(NULL — bob missing, non-zero exit, or no "
                                "flowchart in response)");
            free(s->diagram);
            s->diagram = NULL;
        }
    }
    printf("\nzdoc_ai_annotate: %d of %d symbols annotated\n", annotated, total);
    return (annotated == total && total > 0) ? 0 : 1;
}

/* ----- custom-file mode: diagram one real source file whole ----- */

static int run_custom_file(const char *filepath)
{
    /* Split into dirname + basename so modtree can reconstruct the path; an
     * empty root prefix then yields the path back verbatim. */
    const char *slash = strrchr(filepath, '/');
    char dirbuf[FS_WALK_PATH_MAX];
    const char *base;
    if (slash) {
        size_t dlen = (size_t)(slash - filepath);
        if (dlen >= sizeof dirbuf)
            dlen = sizeof dirbuf - 1;
        memcpy(dirbuf, filepath, dlen);
        dirbuf[dlen] = '\0';
        base = slash + 1;
    } else {
        dirbuf[0] = '.';
        dirbuf[1] = '\0';
        base = filepath;
    }

    fs_walk_root_prefix[0] = '\0'; /* path is absolute-or-relative already */

    modtree_dir_table_t dirs;
    modtree_file_table_t files;
    modtree_dir_table_init(&dirs);
    modtree_file_table_init(&files);
    int d = modtree_intern_dir(&dirs, dirbuf, -1);
    int fi = modtree_intern_file(&files, base, d);

    Symbol sym;
    memset(&sym, 0, sizeof sym);
    sym.name = (char *)base;
    sym.description = "Whole-file diagram";
    sym.line = 1; /* whole file is one "symbol" */

    Module mod;
    memset(&mod, 0, sizeof mod);
    mod.filename = (char *)base;
    mod.symbols = &sym;
    mod.symbolCount = 1;
    mod.pathIndex = fi;

    int rc = run_and_report(&dirs, &files, &mod, 1);
    modtree_dir_table_free(&dirs);
    modtree_file_table_free(&files);
    return rc;
}

/* ----- default mode: three built-in fixtures ----- */

static int run_builtin_fixtures(void)
{
    const char *dir = "/tmp/zdoc_ai_live";
    if (system("mkdir -p /tmp/zdoc_ai_live") != 0) {
        fprintf(stderr, "could not create fixture dir\n");
        return 2;
    }
    if (write_fixture(dir, "sample.plx", PLX_SRC) != 0 ||
        write_fixture(dir, "sample.c", C_SRC) != 0 ||
        write_fixture(dir, "Sample.java", JAVA_SRC) != 0) {
        fprintf(stderr, "could not write fixtures\n");
        return 2;
    }

    strncpy(fs_walk_root_prefix, "/tmp", FS_WALK_PATH_MAX - 1);
    fs_walk_root_prefix[FS_WALK_PATH_MAX - 1] = '\0';

    modtree_dir_table_t dirs;
    modtree_file_table_t files;
    modtree_dir_table_init(&dirs);
    modtree_file_table_init(&files);
    int dd = modtree_intern_dir(&dirs, "zdoc_ai_live", -1);
    int fi_plx = modtree_intern_file(&files, "sample.plx", dd);
    int fi_c = modtree_intern_file(&files, "sample.c", dd);
    int fi_java = modtree_intern_file(&files, "Sample.java", dd);

    Symbol plx_syms[2] = {0};
    plx_syms[0].name = "INITPROC";
    plx_syms[0].description = "Initialise subsystem";
    plx_syms[0].line = 1;
    plx_syms[1].name = "TERMPROC";
    plx_syms[1].description = "Terminate subsystem";
    plx_syms[1].line = 10;

    Symbol c_syms[2] = {0};
    c_syms[0].name = "classify";
    c_syms[0].description = "Sign of n";
    c_syms[0].line = 1;
    c_syms[1].name = "sum_to";
    c_syms[1].description = "Sum 1..n";
    c_syms[1].line = 7;

    Symbol java_syms[2] = {0};
    java_syms[0].name = "classify";
    java_syms[0].description = "Sign of n";
    java_syms[0].line = 2;
    java_syms[1].name = "sumTo";
    java_syms[1].description = "Sum 1..n";
    java_syms[1].line = 6;

    Module modules[3] = {0};
    modules[0].filename = "sample.plx";
    modules[0].symbols = plx_syms;
    modules[0].symbolCount = 2;
    modules[0].pathIndex = fi_plx;
    modules[1].filename = "sample.c";
    modules[1].symbols = c_syms;
    modules[1].symbolCount = 2;
    modules[1].pathIndex = fi_c;
    modules[2].filename = "Sample.java";
    modules[2].symbols = java_syms;
    modules[2].symbolCount = 2;
    modules[2].pathIndex = fi_java;

    int rc = run_and_report(&dirs, &files, modules, 3);
    modtree_dir_table_free(&dirs);
    modtree_file_table_free(&files);
    return rc;
}

int main(int argc, char **argv)
{
    if (argc > 1)
        return run_custom_file(argv[1]);
    return run_builtin_fixtures();
}
