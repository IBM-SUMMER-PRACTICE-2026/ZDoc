/* Unit tests for the md renderer. md-dir_path/md_file_path (path reconstructions from a DxModels parent index links)
and md_render(renders a DxModel built elsewhere by doc_extractor int out_dir as md).
*/
#include "../src/md_renderer.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*Minimal harness: a failed CHECK longjmps out of the current test, so the
rest of that test is skipped but every other test still runs. main()
returns non-zero if any test failed, so `make test` still goes red.*/
static jmp_buf t_jmp;
static int n_failed = 0;

/*Report where and why a check failed, then abandon the current test.*/
static _Noreturn void fail_test(const char *file, int line, const char *fmt, ...) {
    va_list ap;
    fprintf(stderr, "FAIL %s:%d: ", file, line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    longjmp(t_jmp, 1);
}

#define CHECK(cond) do { \
        if (!(cond)) \
            fail_test(__FILE__, __LINE__, "%s", #cond); \
    } while (0)

/*hay must contain needle; on failure shows the start of what was actually
rendered, so the mismatch is visible without opening the output file.*/
#define CHECK_CONTAINS(hay, needle) do { \
        if (strstr((hay), (needle)) == NULL) \
            fail_test(__FILE__, __LINE__, \
                "output does not contain: %s\n  output begins with: %.300s...", \
                (needle), (hay)); \
    } while (0)

/*hay must NOT contain needle; on failure shows where it turned up.*/
#define CHECK_NOT_CONTAINS(hay, needle) do { \
        const char *found_ = strstr((hay), (needle)); \
        if (found_ != NULL) \
            fail_test(__FILE__, __LINE__, \
                "output must not contain: %s\n  found at offset %ld: %.120s...", \
                (needle), (long)(found_ - (hay)), found_); \
    } while (0)

/*actual must equal expected exactly; on failure shows both.*/
#define CHECK_STREQ(actual, expected) do { \
        if (strcmp((actual), (expected)) != 0) \
            fail_test(__FILE__, __LINE__, \
                "expected \"%s\"\n  got      \"%s\"", (expected), (actual)); \
    } while (0)

/*Run one test call, reporting whether it passed and carrying on either way.*/
#define RUN(call) do { \
        if (setjmp(t_jmp) == 0) { \
            call; \
            printf("%s passed\n", #call); \
        } else { \
            n_failed++; \
            printf("%s FAILED\n", #call); \
        } \
    } while (0)

/*Read the whole contents of path into a NULL terminated heap buffer.
Caller frees. Fails the test immidiately if the file cant be read.*/
static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL)
        fail_test(__FILE__, __LINE__, "cannot open rendered output: %s", path);
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)len + 1);
    CHECK(buf != NULL);
    size_t n = fread(buf, 1, (size_t)len, f);
    buf[n] = '\0';
    fclose(f);
    return buf;
}


//dir_index == -1 (the root) reconstructs to the empty string
static void test_md_dir_path_root(void) {
    DxDir dirs[] = { { .name = "src", .parent_index = -1} };
    DxModel m = {.dirs = dirs, .dir_count = 1, .files = NULL, .file_count = 0};

    char out[64];
    CHECK(md_dir_path(&m, -1, out, sizeof out) == 0);
    CHECK_STREQ(out, "");
}

//A nested directory reconstructs root first joined with '/'
static void test_md_dir_path_nested(void) {
    DxDir dirs[] = {
        {.name = "src", .parent_index = -1},
        {.name = "util", .parent_index = 0},
    };
    DxModel m = {.dirs = dirs, .dir_count = 2, .files =NULL, .file_count = 0};

    char out[64];
    CHECK(md_dir_path(&m, 1, out, sizeof out) == 0);
    CHECK_STREQ(out, "src/util");
}

//A destionation buffer too small to hold the reconstructed path fails cleanly instead truncating or overflowing
static void test_md_dir_path_buffer_too_small(void) {
    DxDir dirs[] = {
        {.name = "src", .parent_index = -1},
        {.name = "util", .parent_index = 0},
    };
    DxModel m = {.dirs = dirs, .dir_count = 2, .files = NULL, .file_count = 0};

    
    char out[4]; // src/util need 9 bytes
    CHECK(md_dir_path(&m, 1, out, sizeof out) == -1);
}

/*A files path is its parent directorys path plus its own name 
and a file directly at the root (parent_dir_index == -1) is just its name
with no leading '/'. */
static void test_md_file_path(void) {
    DxDir dirs[] = {
        {.name = "src", .parent_index = -1},
        {.name = "util", .parent_index = 0},
    };
    DxFile files[] = {
        {.name = "Helper.java", .parent_dir_index = 1, .language = "java",
        .symbols = NULL, .symbol_count = 0, .error = 0},
        {.name = "README", .parent_dir_index = -1, .language = NULL,
        .symbols = NULL, .symbol_count = 0, .error = 0},
    };
    DxModel m = {.dirs = dirs, .dir_count = 2, .files = files, .file_count = 2};

    char out[64];
    CHECK(md_file_path(&m, 0, out, sizeof out) == 0);
    CHECK_STREQ(out, "src/util/Helper.java");

    CHECK(md_file_path(&m, 1, out, sizeof out) == 0);
    CHECK_STREQ(out, "README");
}

/* A fully populated symbol renders its signature (fenced with the file's
language),parameter table,returns and notes a minimal symbol (only
name and signature) omits every optional section.*/
static void test_module_symbol_rendering(const char *out_dir) {
    char path[512];
    snprintf(path, sizeof path, "%s/module_symbols", out_dir);

    DxParam params[] = { { .name = "args", .desc = "command-line arguments" } };
    DxSymbol symbols[] = {
        {
            .kind = "function", .line = 10, .name = "main",
            .signature = "public static void main(String[] args)",
            .brief = "Program entry point",
            .params = params, .param_count = 1,
            .returns = "void", .notes = "Prints a greeting.",
            .diagram = NULL, .refs = NULL, .ref_count = 0,
        },
        {
            .kind = "function", .line = 20, .name = "helper",
            .signature = "private static void helper()",
            .brief = NULL, .params = NULL, .param_count = 0,
            .returns = NULL, .notes = NULL,
            .diagram = NULL, .refs = NULL, .ref_count = 0,
        },
    };
    DxDir dirs[] = { { .name = "src", .parent_index = -1 } };
    DxFile files[] = {
        { .name = "Main.java", .parent_dir_index = 0, .language = "java",
          .symbols = symbols, .symbol_count = 2, .error = 0 },
    };
    DxModel m = { .dirs = dirs, .dir_count = 1, .files = files, .file_count = 1 };

    CHECK(md_render(&m, path, "Test Docs") == 0);

    char module_path[600];
    snprintf(module_path, sizeof module_path, "%s/src/Main.md", path);
    char *md = read_file(module_path);

    CHECK_CONTAINS(md, "# Module: src/Main.java");

    CHECK_CONTAINS(md, "<summary><strong>main</strong> \xe2\x80\x94 Program entry point</summary>");
    CHECK_CONTAINS(md, "```java\npublic static void main(String[] args)\n```");
    CHECK_CONTAINS(md, "| args | command-line arguments |");
    CHECK_CONTAINS(md, "**Returns**\nvoid");
    CHECK_CONTAINS(md, "**Notes**\nPrints a greeting.");

    CHECK_CONTAINS(md, "<summary><strong>helper</strong></summary>");
    CHECK_CONTAINS(md, "```java\nprivate static void helper()\n```");
    /* helper has no params/returns/notes - the sections above must not
    reappear a second time for it */
    char *first_returns = strstr(md, "**Returns**");
    CHECK(first_returns != NULL);
    CHECK_NOT_CONTAINS(first_returns + 1, "**Returns**");

    free(md);
}

/*A file with zero documented symbols and a file the parser failed on
(error == 1) render identically just the module header, since
md_render has no separate handling for error at all.*/
static void test_empty_and_error_files_same_shape(const char *out_dir) {
    char path[512];
    snprintf(path, sizeof path, "%s/empty_and_error", out_dir);

    DxDir dirs[] = { { .name = "src", .parent_index = -1 } };
    DxFile files[] = {
        { .name = "Empty.java", .parent_dir_index = 0, .language = "java",
          .symbols = NULL, .symbol_count = 0, .error = 0 },
        { .name = "Broken.c", .parent_dir_index = 0, .language = "c",
          .symbols = NULL, .symbol_count = 0, .error = 1 },
    };
    DxModel m = { .dirs = dirs, .dir_count = 1, .files = files, .file_count = 2 };

    CHECK(md_render(&m, path, "Test Docs") == 0);

    char empty_path[600], broken_path[600];
    snprintf(empty_path, sizeof empty_path, "%s/src/Empty.md", path);
    snprintf(broken_path, sizeof broken_path, "%s/src/Broken.md", path);
    char *empty_md = read_file(empty_path);
    char *broken_md = read_file(broken_path);

    CHECK_STREQ(empty_md, "# Module: src/Empty.java\n\n");
    CHECK_STREQ(broken_md, "# Module: src/Broken.c\n\n");

    free(empty_md);
    free(broken_md);
}

/*index.md lists nested directories in bold and files as links to their
rendered .md page, both indented one level per directory of depth.*/
static void test_index_tree(const char *out_dir) {
    char path[512];
    snprintf(path, sizeof path, "%s/index_tree", out_dir);

    DxDir dirs[] = {
        { .name = "src", .parent_index = -1 },
        { .name = "util", .parent_index = 0 },
    };
    DxFile files[] = {
        { .name = "Main.java", .parent_dir_index = 0, .language = "java",
          .symbols = NULL, .symbol_count = 0, .error = 0 },
        { .name = "Util.java", .parent_dir_index = 1, .language = "java",
          .symbols = NULL, .symbol_count = 0, .error = 0 },
    };
    DxModel m = { .dirs = dirs, .dir_count = 2, .files = files, .file_count = 2 };

    CHECK(md_render(&m, path, "Test Docs") == 0);

    char index_path[600];
    snprintf(index_path, sizeof index_path, "%s/index.md", path);
    char *idx = read_file(index_path);

    CHECK_CONTAINS(idx, "# Test Docs");
    CHECK_CONTAINS(idx, "- **src/**");
    CHECK_CONTAINS(idx, "  - **util/**");
    CHECK_CONTAINS(idx, "    - [Util.java](src/util/Util.md)");
    CHECK_CONTAINS(idx, "  - [Main.java](src/Main.md)");

    free(idx);
}

//A destination buffer too small for md_file_path's reconstructed path fails cleanly instead of truncating or overflowing
static void test_md_file_path_buffer_too_small(void) {
    DxDir dirs[] = {
        {.name = "src", .parent_index = -1},
        {.name = "util", .parent_index = 0},
    };
    DxFile files[] = {
        {.name = "Helper.java", .parent_dir_index = 1, .language = "java",
         .symbols = NULL, .symbol_count = 0, .error = 0},
    };
    DxModel m = {.dirs = dirs, .dir_count = 2, .files = files, .file_count = 1};

    char out[10]; // "src/util/Helper.java" needs 22 bytes
    CHECK(md_file_path(&m, 0, out, sizeof out) == -1);
}

//A NULL title falls back to the default "Documentation" heading in index.md
static void test_md_default_title(const char *out_dir) {
    char path[512];
    snprintf(path, sizeof path, "%s/default_title", out_dir);

    DxDir dirs[] = { {.name = "src", .parent_index = -1} };
    DxModel m = {.dirs = dirs, .dir_count = 1, .files = NULL, .file_count = 0};

    CHECK(md_render(&m, path, NULL) == 0);

    char index_path[600];
    snprintf(index_path, sizeof index_path, "%s/index.md", path);
    char *idx = read_file(index_path);

    CHECK_CONTAINS(idx, "# Documentation\n\n");

    free(idx);
}

/*write_symbol does no escaping - backticks and pipes in a name, brief or
signature pass straight through into the Markdown. This documents current
behaviour (a name/signature containing "```" could break a code fence);
if escaping is added later this test's assertions need updating.*/
static void test_md_raw_passthrough(const char *out_dir) {
    char path[512];
    snprintf(path, sizeof path, "%s/raw_passthrough", out_dir);

    DxSymbol symbols[] = {
        {
            .kind = "function", .line = 1, .name = "weird`name",
            .signature = "void f() // has a ` backtick and a | pipe",
            .brief = "uses `code` and | pipes",
            .params = NULL, .param_count = 0,
            .returns = NULL, .notes = NULL, .diagram = NULL,
            .refs = NULL, .ref_count = 0,
        },
    };
    DxDir dirs[] = { {.name = "src", .parent_index = -1} };
    DxFile files[] = {
        {.name = "Weird.c", .parent_dir_index = 0, .language = "c",
         .symbols = symbols, .symbol_count = 1, .error = 0},
    };
    DxModel m = {.dirs = dirs, .dir_count = 1, .files = files, .file_count = 1};

    CHECK(md_render(&m, path, "Raw") == 0);

    char module_path[600];
    snprintf(module_path, sizeof module_path, "%s/src/Weird.md", path);
    char *md = read_file(module_path);

    CHECK_CONTAINS(md, "weird`name");
    CHECK_CONTAINS(md, "uses `code` and | pipes");
    CHECK_CONTAINS(md, "has a ` backtick and a | pipe");

    free(md);
}

/*md_render has no notion of diagrams or cross-references - a symbol that
carries them renders identically to one that doesn't, since write_symbol
never reads s->diagram or s->refs.*/
static void test_md_diagram_refs_ignored(const char *out_dir) {
    char path[512];
    snprintf(path, sizeof path, "%s/diagram_refs_ignored", out_dir);

    char *refs[] = { "other" };
    DxSymbol symbols[] = {
        {
            .kind = "function", .line = 1, .name = "f",
            .signature = "void f()",
            .brief = NULL, .params = NULL, .param_count = 0,
            .returns = NULL, .notes = NULL,
            .diagram = "flowchart TD A --> B",
            .refs = refs, .ref_count = 1,
        },
    };
    DxDir dirs[] = { {.name = "src", .parent_index = -1} };
    DxFile files[] = {
        {.name = "F.c", .parent_dir_index = 0, .language = "c",
         .symbols = symbols, .symbol_count = 1, .error = 0},
    };
    DxModel m = {.dirs = dirs, .dir_count = 1, .files = files, .file_count = 1};

    CHECK(md_render(&m, path, "Test Docs") == 0);

    char module_path[600];
    snprintf(module_path, sizeof module_path, "%s/src/F.md", path);
    char *md = read_file(module_path);

    CHECK_NOT_CONTAINS(md, "flowchart");
    CHECK_NOT_CONTAINS(md, "other");

    free(md);
}

/*md_render fails cleanly (returns -1) when it can't create/open its
output files - e.g. a path component collides with an existing plain
file. Relies on out_dir already existing from earlier tests in this run.*/
static void test_md_write_failure(const char *out_dir) {
    char blocker_file[600];
    snprintf(blocker_file, sizeof blocker_file, "%s/blocker_write_failure", out_dir);
    FILE *f = fopen(blocker_file, "wb");
    CHECK(f != NULL);
    fclose(f);

    char bad_path[700];
    snprintf(bad_path, sizeof bad_path, "%s/sub", blocker_file);

    DxDir dirs[] = { {.name = "src", .parent_index = -1} };
    DxModel m = {.dirs = dirs, .dir_count = 1, .files = NULL, .file_count = 0};

    CHECK(md_render(&m, bad_path, "Should Fail") == -1);
}

/*A render that fails partway removes what it already wrote - the caller
gets the error and no partial render is left behind. Good.java renders to
src/Good.md first; Bad.java's parent "directory" is also named Good.md,
so writing under src/Good.md/... collides with that file and fails.*/
static void test_md_failed_render_cleans_up(const char *out_dir) {
    char path[512];
    snprintf(path, sizeof path, "%s/cleanup", out_dir);

    DxDir dirs[] = {
        {.name = "src", .parent_index = -1},
        {.name = "Good.md", .parent_index = 0},
    };
    DxFile files[] = {
        {.name = "Good.java", .parent_dir_index = 0, .language = "java",
         .symbols = NULL, .symbol_count = 0, .error = 0},
        {.name = "Bad.java", .parent_dir_index = 1, .language = "java",
         .symbols = NULL, .symbol_count = 0, .error = 0},
    };
    DxModel m = {.dirs = dirs, .dir_count = 2, .files = files, .file_count = 2};

    CHECK(md_render(&m, path, "Cleanup") == -1);

    /* the module file that WAS written successfully must be gone again */
    char good_path[600];
    snprintf(good_path, sizeof good_path, "%s/src/Good.md", path);
    FILE *f = fopen(good_path, "rb");
    CHECK(f == NULL);

    char index_path[600];
    snprintf(index_path, sizeof index_path, "%s/index.md", path);
    f = fopen(index_path, "rb");
    CHECK(f == NULL);
}

int main(int argc, char **argv) {
    /* volatile: read again after a longjmp back into this frame */
    const char *volatile out_dir = argc > 1 ? argv[1] : "tests/tmp";

    RUN(test_md_dir_path_root());
    RUN(test_md_dir_path_nested());
    RUN(test_md_dir_path_buffer_too_small());
    RUN(test_md_file_path());
    RUN(test_md_file_path_buffer_too_small());
    RUN(test_module_symbol_rendering(out_dir));
    RUN(test_empty_and_error_files_same_shape(out_dir));
    RUN(test_index_tree(out_dir));
    RUN(test_md_default_title(out_dir));
    RUN(test_md_raw_passthrough(out_dir));
    RUN(test_md_diagram_refs_ignored(out_dir));
    RUN(test_md_write_failure(out_dir));
    RUN(test_md_failed_render_cleans_up(out_dir));

    if (n_failed > 0) {
        printf("\n%d md_renderer test(s) FAILED.\n", n_failed);
        return 1;
    }
    printf("\nAll md_renderer checks passed.\n");
    return 0;
}