/* Unit tests for the html renderer. 
  Renders a DxModel (built by the extractor) into out_dir/index.html
*/

#include "../html_renderer.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*Read the contents of path into a NULL terminated heap buffer. 
Caller frees. Fails the test if the file cant be read.
*/
static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    assert(f != NULL);
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)len + 1);
    asset(buf != NULL);
    size_t n = fread(buf, 1, (size_t)len,f);
    buf[n] = '\0';
    fclose(f);
    return buf;
}

/* A directory tree with one nested subdirectory renderes both levels 
as <details class="dir"> nodes and a file with zero symbols (but no parser error) is reported as having none.
*/
static void test_nested_dir_tree(const char *out_dir) {
    char path[512];
    snprintf(path, sizeof path, "%s/nested", out_dir);

    DxDir dirs[] = {
        {.name = "src",.parent_index = -1},
        {.name = "util", .parent_index = 0},
    };

    DxFile files[] = {
        {.name = "Helper.java", .parent_dir_index = 1, .language = "java",
         .symbols = NULL, .symbol_count = 0, .error = 0},
    };

    DxModel m = {.dirs = dirs,.dir_count = 2, .files = files,.file_count = 1};

    assert(html_render(&m, path, "Nested") == 0);

    char index[600];
    snprintf(index, sizeof index, "%s/index.html", path);
    char *html = read_file(index);

    assert(strstr(html, "<summary>src/</summary>") != NULL);
    assert(strstr(html, "<summary>util/</summary>") != NULL);
    assert(strstr(html, "<summary>Helper.java</summary>") != NULL);
    assert(strstr(html, "No documented symbols") != NULL);

    free(html);
    printf("test_nested_dir_tree passed\n");
}


//A file the parser failed on (error == 1) is reported instead of being treated as having zero documented symbols
static void test_error_file(const char *out_dir) {
    char path[512];
    snprintf(path, sizeof path, "%s/error_file", out_dir);

    DxDir dirs[] = { {.name = "src",.parent_index = -1 } };
    DxFile files[] = {
        {.name = "Broken.c", .parent_dir_index = 0, .language = "c",
        .symbols = NULL, .symbol_count = 0, .error = 1 }, 
    };
    DxModel m = {.dirs = dirs, .dir_count = 1, .files = files, .file_count = 1};

    assert(html_render(&m, path, "Errors") == 0);

    char index[600];
    snprintf(index, sizeof index, "%s/index.html", path);
    char *html = read_file(index);

    assert(strstr(html, "Parser failed for this file") != NULL);
    assert(strstr(html, "No documented symbols.") == NULL);

    free(html);
    printf("test_error_file passed\n");
}


/*A fully populated symbol renders every optional section: brief ,parameters table, returns, notes,
block diagram and cross-references and the two scripts (hash reveal, Mermaid) are only emmited because this model actually carries ref/diagrams.*/

static void test_full_symbol(const char *out_dir) {
    char path[512];
    snprintf(path, sizeof path, "%s/full_symbol", out_dir);

    DxParam params[] = { {.name = "args", .desc ="command-line arguments"} };
    char *refs[] = { "helper" };
    DxSymbol symbols[] = {
        {
            .kind = "function", .line = 10, .name = "main",
            .signature = "public static void main(String[] args)",
            .brief = "Program entry point",
            .params = params, .param_count = 1,
            .returns = "void", .notes = "Prints a greeting.",
            .diagram = "flowchart TD Start --- End",
            .refs = refs, .ref_count = 1,
        },
    };

    DxDir dirs[] = { {.name = "src", .parent_index = -1 } };

    DxFile files[] = {
        { .name = "Main.java", .parent_dir_index = 0, .language = "java",
        .symbols = symbols, .symbol_count = 1, .error = 0},
    };

    DxModel m = {.dirs = dirs, .dir_count = 1,.files = files, .file_count = 1};

    assert(html_render(&m, path, "Full Symbol") == 0);

    char index[600];
    snprintf(index, sizeof index, "%s/index.html", path);
    char *html = read_file(index);

    assert(strstr(html, "id=\"sym-main\"") != NULL);
    assert(strstr(html, "<span class=\"brief\">\xe2\x80\x94 Program entry point</span>") != NULL);
    assert(strstr(html, "<code class=\"language-java\">public static void main(String[] args)</code>") != NULL);
    assert(strstr(html, "<td><code>args</code></td><td>command-line arguments</td>") != NULL);
    assert(strstr(html, "<p class=\"h\">Returns</p>\n<p>void</p>") != NULL);
    assert(strstr(html, "<p>Prints a greeting.</p>") != NULL);
    assert(strstr(html, "<pre class=\"mermaid\">flowchart TD Start --- End</pre>") != NULL);
    assert(strstr(html, "href=\"#sym-helper\"") != NULL);
    assert(strstr(html, "window.addEventListener('hashchange',reveal)") != NULL);
    assert(strstr(html, "cdn.jsdelivr.net/npm/mermaid") != NULL);

    free(html);
    printf("test_full_symbol_passed\n");
}


/*A minamal symbol (only name and signature) omits every optional section and with no refs/diagrams anywhere in the model, 
neither the hash-reveal nore the Mermaid script is emitted at all
*/

static void test_minimal_symbol(const char *out_dir) {
    char path[512];
    snprintf(path, sizeof path, "%s/minimal_symbol", out_dir);

    DxSymbol symbols[] = {
        {
            .kind = "function", .line = 20, .name = "helper",
            .signature = "private static void helper()",
            .brief = NULL, .params = NULL, .param_count = 0,
            .returns = NULL, .notes = NULL, .diagram = NULL,
            .refs = NULL, .ref_count = 0,
        },
    };

    DxDir dirs[] = { {.name = "src", .parent_index = -1} };

    DxFile files[] = {
        {.name = "Main.java", .parent_dir_index = 0, .language = "java",
        .symbols = symbols, .symbol_count = 1., .error = 0}
    };

    DxModel m = { .dirs = dirs, .dir_count = 1, .files = files, .file_count = 1};

    assert(html_render(&m,path, "Minimal Symbol") == 0);

    char index[600];
    snprintf(index, sizeof index, "%s/index.html", path);
    char *html = read_file(index);

    assert(strstr(html, "id=\"sym-helper\"") != NULL);
    assert(strstr(html, "<code class=\"language-java\">private static void helper()</code>") != NULL);
    assert(strstr(html, "class=\"brief\"") == NULL);
    assert(strstr(html, "Parameters</p>") == NULL);
    assert(strstr(html, "Returns</p>") == NULL);
    assert(strstr(html, "Notes</p>") == NULL);
    assert(strstr(html, "Block Diagram</p>") == NULL);
    assert(strstr(html, "Cross-references</p>") == NULL);
    assert(strstr(html, "hashchange") == NULL);
    assert(strstr(html, "cdn.jsdelivr.net/npm/mermaid") == NULL);
    assert(strstr(html, "<pre class=\"mermaid\">") == NULL);
    assert(strstr(html, "pre.mermaid{display:none}") == NULL);

    free(html);
    printf("test_minal_symbol passed\n");
}


//A NULL title falls back to the default "Documentation" heading.
static void test_default_title(const char *out_dir) {
    char path[512];
    snprintf(path, sizeof path, "%s/default_title", out_dir);

    DxDir dirs[] = { {.name = "src", .parent_index = -1 } };
    DxModel m = {.dirs = dirs, .dir_count = 1, .files = NULL, .file_count = 0};

    assert(html_render(&m, path, NULL) == 0);

    char index[600];
    snprintf(index, sizeof index, "%s/index.html", path); 
    char *html = read_file(index);

    assert(strstr(html,"<title>Documentation</title>") != NULL);
    assert(strstr(html, "<h1>Documentation</h1>") != NULL);

    free(html);
    printf("test_default_title passed\n");
}

int main(int argc, char **argv) {
    const char *out_dir = argc > 1 ? argv[1] : "tests/tmp";
    
    test_nested_dir_tree(out_dir);
    test_error_file(out_dir);
    test_full_symbol(out_dir);
    test_minimal_symbol(out_dir);
    test_default_title(out_dir);

    printf("\nAll html_renderer checks passed.\n");
    return 0;
}