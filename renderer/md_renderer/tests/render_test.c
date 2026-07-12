/* Unit tests for the md renderer. md-dir_path/md_file_path (path reconstructions from a DxModels parent index links)
and md_render(renders a DxModel built elsewhere by doc_extractor int out_dir as md).
*/
#include "../src/md_renderer.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*Read the whole contents of path into a NULL terminated heap buffer.
Caller frees. Fails the test immidiately if the file cant be read.*/
static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    assert(f != NULL);
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)len + 1);
    assert(buf != NULL);
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
    assert(md_dir_path(&m, -1, out, sizeof out));
    assert(strcmp(out, "") == 0);

    printf("test_md_dir_path_root passed\n");
}

//A nested directory reconstructs root first joined with '/'
static void test_md_dir_path_nested(void) {
    DxDir dirs[] = {
        {.name = "src", .parent_index = -1},
        {.name = "util", .parent_index = 0},
    };
    DxModel m = {.dirs = dirs, .dir_count = 2, .files =NULL, .file_count = 0};

    char out[64];
    assert(md_dir_path(&m, 1, out, sizeof out));
    assert(strcmp(out, "src/util") == 0);

    printf("test_md_dir_nested passed\n");
}

//A destionation buffer too small to hold the reconstructed path fails cleanly instead truncating or overflowing
static void test_md_dir_path_buffer_too_small(void) {
    DxDir dirs[] = {
        {.name = "src", .parent_index = -1},
        {.name = "util", .parent_index = 0},
    };
    DxModel m = {.dirs = dirs, .dir_count = 2, .files = NULL, .file_count = 0};

    
    char out[4]; // src/util need 9 bytes
    assert(md_dir_path(&m, 1, out, sizeof out) == -1);

    printf("test_md_dir_path_buffer_too_small passed\n");
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
    assert(md_file_path(&m, 0, out, sizeof out) == 0);
    assert(strcmp(out, "src/util/Helper.java") == 0);

    assert(md_file_path(&m, 1, out, sizeof out) == 0);
    assert(strcmp(out, "README") == 0);

    printf("test_md_file_path passed\n");
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

    assert(md_render(&m, path, "Test Docs") == 0);

    char module_path[600];
    snprintf(module_path, sizeof module_path, "%s/src/Main.md", path);
    char *md = read_file(module_path);

    assert(strstr(md, "# Module: src/Main.java") != NULL);

    assert(strstr(md, "<summary><strong>main</strong> \xe2\x80\x94 Program entry point</summary>") != NULL);
    assert(strstr(md, "```java\npublic static void main(String[] args)\n```") != NULL);
    assert(strstr(md, "| args | command-line arguments |") != NULL);
    assert(strstr(md, "**Returns**\nvoid") != NULL);
    assert(strstr(md, "**Notes**\nPrints a greeting.") != NULL);

    assert(strstr(md, "<summary><strong>helper</strong></summary>") != NULL);
    assert(strstr(md, "```java\nprivate static void helper()\n```") != NULL);
    /* helper has no params/returns/notes - the sections above must not
    reappear a second time for it */
    char *first_returns = strstr(md, "**Returns**");
    assert(first_returns != NULL);
    assert(strstr(first_returns + 1, "**Returns**") == NULL);

    free(md);
    printf("test_module_symbol_rendering passed\n");
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

    assert(md_render(&m, path, "Test Docs") == 0);

    char empty_path[600], broken_path[600];
    snprintf(empty_path, sizeof empty_path, "%s/src/Empty.md", path);
    snprintf(broken_path, sizeof broken_path, "%s/src/Broken.md", path);
    char *empty_md = read_file(empty_path);
    char *broken_md = read_file(broken_path);

    assert(strcmp(empty_md, "# Module: src/Empty.java\n\n") == 0);
    assert(strcmp(broken_md, "# Module: src/Broken.c\n\n") == 0);

    free(empty_md);
    free(broken_md);
    printf("test_empty_and_error_files_same_shape passed\n");
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

    assert(md_render(&m, path, "Test Docs") == 0);

    char index_path[600];
    snprintf(index_path, sizeof index_path, "%s/index.md", path);
    char *idx = read_file(index_path);

    assert(strstr(idx, "# Test Docs") != NULL);
    assert(strstr(idx, "- **src/**") != NULL);
    assert(strstr(idx, "  - **util/**") != NULL);
    assert(strstr(idx, "    - [Util.java](src/util/Util.md)") != NULL);
    assert(strstr(idx, "  - [Main.java](src/Main.md)") != NULL);

    free(idx);
    printf("test_index_tree passed\n");
}

int main(int argc, char **argv) {
    const char *out_dir = argc > 1 ? argv[1] : "tests/tmp";

    test_md_dir_path_root();
    test_md_dir_path_nested();
    test_md_dir_path_buffer_too_small();
    test_md_file_path();
    test_module_symbol_rendering(out_dir);
    test_empty_and_error_files_same_shape(out_dir);
    test_index_tree(out_dir);

    printf("\nAll md_renderer checks passed.\n");
    return 0;
}