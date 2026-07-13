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
    assert(buf != NULL);
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

/*Names, briefs and signatures are HTML-escaped, so raw '<', '>', '&', '"'
from source text never reach the page unescaped.*/
static void test_html_escaping(const char *out_dir) {
    char path[512];
    snprintf(path, sizeof path, "%s/escaping", out_dir);

    DxSymbol symbols[] = {
        {
            .kind = "function", .line = 1, .name = "a<b",
            .signature = "void f(int x) /* <script>\"&\" */",
            .brief = "uses <tags> & \"quotes\"",
            .params = NULL, .param_count = 0,
            .returns = NULL, .notes = NULL, .diagram = NULL,
            .refs = NULL, .ref_count = 0,
        },
    };
    DxDir dirs[] = { {.name = "src", .parent_index = -1} };
    DxFile files[] = {
        {.name = "Weird<File>.c", .parent_dir_index = 0, .language = "c",
         .symbols = symbols, .symbol_count = 1, .error = 0},
    };
    DxModel m = {.dirs = dirs, .dir_count = 1, .files = files, .file_count = 1};

    assert(html_render(&m, path, "Escaping") == 0);

    char index[600];
    snprintf(index, sizeof index, "%s/index.html", path);
    char *html = read_file(index);

    assert(strstr(html, "Weird&lt;File&gt;.c") != NULL);
    assert(strstr(html, "<summary>Weird<File>.c</summary>") == NULL);
    assert(strstr(html, "uses &lt;tags&gt; &amp; &quot;quotes&quot;") != NULL);
    assert(strstr(html, "&lt;script&gt;&quot;&amp;&quot;") != NULL);
    assert(strstr(html, "<script>\"&\"") == NULL);

    free(html);
    printf("test_html_escaping passed\n");
}

/*A directory whose parent_index is out of range or points at itself is
promoted to a root instead of being nested under a bogus/looping parent
(the extractor's output is treated as untrusted input here).*/
static void test_malformed_parent_index(const char *out_dir) {
    char path[512];
    snprintf(path, sizeof path, "%s/malformed_parent", out_dir);

    DxDir dirs[] = {
        {.name = "root_a", .parent_index = -1},
        {.name = "bad_parent", .parent_index = 99},  /* out of range */
        {.name = "self_ref", .parent_index = 2},      /* points at itself */
    };
    DxModel m = {.dirs = dirs, .dir_count = 3, .files = NULL, .file_count = 0};

    assert(html_render(&m, path, "Malformed") == 0);

    char index[600];
    snprintf(index, sizeof index, "%s/index.html", path);
    char *html = read_file(index);

    assert(strstr(html, "<summary>root_a/</summary>") != NULL);
    assert(strstr(html, "<summary>bad_parent/</summary>") != NULL);
    assert(strstr(html, "<summary>self_ref/</summary>") != NULL);
    /* root_a has no children - if bad_parent/self_ref had been nested
       under it instead of promoted to root, this exact empty-body
       substring wouldn't appear. */
    assert(strstr(html, "<summary>root_a/</summary><ul>\n</ul></details></li>") != NULL);

    free(html);
    printf("test_malformed_parent_index passed\n");
}

/*A model with a diagram but no refs emits the Mermaid script (and its
noscript CSS) but not the hash-reveal script.*/
static void test_diagram_without_refs(const char *out_dir) {
    char path[512];
    snprintf(path, sizeof path, "%s/diagram_only", out_dir);

    DxSymbol symbols[] = {
        {
            .kind = "function", .line = 1, .name = "f",
            .signature = "void f()",
            .brief = NULL, .params = NULL, .param_count = 0,
            .returns = NULL, .notes = NULL,
            .diagram = "flowchart TD A --> B",
            .refs = NULL, .ref_count = 0,
        },
    };
    DxDir dirs[] = { {.name = "src", .parent_index = -1} };
    DxFile files[] = {
        {.name = "F.c", .parent_dir_index = 0, .language = "c",
         .symbols = symbols, .symbol_count = 1, .error = 0},
    };
    DxModel m = {.dirs = dirs, .dir_count = 1, .files = files, .file_count = 1};

    assert(html_render(&m, path, "Diagram Only") == 0);

    char index[600];
    snprintf(index, sizeof index, "%s/index.html", path);
    char *html = read_file(index);

    assert(strstr(html, "cdn.jsdelivr.net/npm/mermaid") != NULL);
    assert(strstr(html, "pre.mermaid{display:none}") != NULL);
    assert(strstr(html, "hashchange") == NULL);

    free(html);
    printf("test_diagram_without_refs passed\n");
}

/*A model with cross-references but no diagram emits the hash-reveal
script, but not Mermaid or its noscript CSS.*/
static void test_refs_without_diagram(const char *out_dir) {
    char path[512];
    snprintf(path, sizeof path, "%s/refs_only", out_dir);

    char *refs[] = { "other" };
    DxSymbol symbols[] = {
        {
            .kind = "function", .line = 1, .name = "f",
            .signature = "void f()",
            .brief = NULL, .params = NULL, .param_count = 0,
            .returns = NULL, .notes = NULL, .diagram = NULL,
            .refs = refs, .ref_count = 1,
        },
    };
    DxDir dirs[] = { {.name = "src", .parent_index = -1} };
    DxFile files[] = {
        {.name = "F.c", .parent_dir_index = 0, .language = "c",
         .symbols = symbols, .symbol_count = 1, .error = 0},
    };
    DxModel m = {.dirs = dirs, .dir_count = 1, .files = files, .file_count = 1};

    assert(html_render(&m, path, "Refs Only") == 0);

    char index[600];
    snprintf(index, sizeof index, "%s/index.html", path);
    char *html = read_file(index);

    assert(strstr(html, "hashchange") != NULL);
    assert(strstr(html, "cdn.jsdelivr.net/npm/mermaid") == NULL);
    assert(strstr(html, "pre.mermaid{display:none}") == NULL);

    free(html);
    printf("test_refs_without_diagram passed\n");
}

/*NULL names for a directory, file and symbol all fall back to the literal
"(unnamed)" instead of printing an empty tag, and a nameless symbol gets
no id attribute on its <details> at all (there'd be nothing for a
cross-reference link to target).*/
static void test_unnamed_fallbacks(const char *out_dir) {
    char path[512];
    snprintf(path, sizeof path, "%s/unnamed", out_dir);

    DxSymbol symbols[] = {
        {
            .kind = "function", .line = 1, .name = NULL,
            .signature = "void f()",
            .brief = NULL, .params = NULL, .param_count = 0,
            .returns = NULL, .notes = NULL, .diagram = NULL,
            .refs = NULL, .ref_count = 0,
        },
    };
    DxDir dirs[] = { {.name = NULL, .parent_index = -1} };
    DxFile files[] = {
        {.name = NULL, .parent_dir_index = 0, .language = "c",
         .symbols = symbols, .symbol_count = 1, .error = 0},
    };
    DxModel m = {.dirs = dirs, .dir_count = 1, .files = files, .file_count = 1};

    assert(html_render(&m, path, "Unnamed") == 0);

    char index[600];
    snprintf(index, sizeof index, "%s/index.html", path);
    char *html = read_file(index);

    assert(strstr(html, "<summary>(unnamed)/</summary>") != NULL);
    assert(strstr(html, "<summary>(unnamed)</summary>") != NULL);
    assert(strstr(html, "<details class=\"sym\"><summary><code>(unnamed)</code>") != NULL);

    free(html);
    printf("test_unnamed_fallbacks passed\n");
}

/*Multiple sibling directories and files at the same level are all
rendered, in the same order they appear in the model's tables.*/
static void test_sibling_order(const char *out_dir) {
    char path[512];
    snprintf(path, sizeof path, "%s/siblings", out_dir);

    DxDir dirs[] = {
        {.name = "alpha", .parent_index = -1},
        {.name = "beta", .parent_index = -1},
        {.name = "gamma", .parent_index = -1},
    };
    DxFile files[] = {
        {.name = "One.c", .parent_dir_index = -1, .language = "c",
         .symbols = NULL, .symbol_count = 0, .error = 0},
        {.name = "Two.c", .parent_dir_index = -1, .language = "c",
         .symbols = NULL, .symbol_count = 0, .error = 0},
    };
    DxModel m = {.dirs = dirs, .dir_count = 3, .files = files, .file_count = 2};

    assert(html_render(&m, path, "Siblings") == 0);

    char index[600];
    snprintf(index, sizeof index, "%s/index.html", path);
    char *html = read_file(index);

    const char *a = strstr(html, "<summary>alpha/</summary>");
    const char *b = strstr(html, "<summary>beta/</summary>");
    const char *g = strstr(html, "<summary>gamma/</summary>");
    const char *one = strstr(html, "<summary>One.c</summary>");
    const char *two = strstr(html, "<summary>Two.c</summary>");
    assert(a && b && g && one && two);
    assert(a < b && b < g && g < one && one < two);

    free(html);
    printf("test_sibling_order passed\n");
}

/*A file with more than one symbol renders each as its own
<details class="sym">, in table order.*/
static void test_multiple_symbols_in_file(const char *out_dir) {
    char path[512];
    snprintf(path, sizeof path, "%s/multi_symbol", out_dir);

    DxSymbol symbols[] = {
        {.kind = "function", .line = 1, .name = "first", .signature = "void first()",
         .brief = NULL, .params = NULL, .param_count = 0, .returns = NULL,
         .notes = NULL, .diagram = NULL, .refs = NULL, .ref_count = 0},
        {.kind = "function", .line = 2, .name = "second", .signature = "void second()",
         .brief = NULL, .params = NULL, .param_count = 0, .returns = NULL,
         .notes = NULL, .diagram = NULL, .refs = NULL, .ref_count = 0},
    };
    DxDir dirs[] = { {.name = "src", .parent_index = -1} };
    DxFile files[] = {
        {.name = "Two.c", .parent_dir_index = 0, .language = "c",
         .symbols = symbols, .symbol_count = 2, .error = 0},
    };
    DxModel m = {.dirs = dirs, .dir_count = 1, .files = files, .file_count = 1};

    assert(html_render(&m, path, "Multi Symbol") == 0);

    char index[600];
    snprintf(index, sizeof index, "%s/index.html", path);
    char *html = read_file(index);

    const char *first = strstr(html, "id=\"sym-first\"");
    const char *second = strstr(html, "id=\"sym-second\"");
    assert(first && second && first < second);

    free(html);
    printf("test_multiple_symbols_in_file passed\n");
}

/*html_render fails cleanly (returns -1, doesn't crash) when it can't
create/open its output file - e.g. a path component collides with an
existing plain file. Relies on out_dir already existing from earlier
tests in this run.*/
static void test_write_failure(const char *out_dir) {
    char blocker_file[600];
    snprintf(blocker_file, sizeof blocker_file, "%s/blocker_write_failure", out_dir);
    FILE *f = fopen(blocker_file, "wb");
    assert(f != NULL);
    fclose(f);

    char bad_path[700];
    snprintf(bad_path, sizeof bad_path, "%s/sub", blocker_file);

    DxDir dirs[] = { {.name = "src", .parent_index = -1} };
    DxModel m = {.dirs = dirs, .dir_count = 1, .files = NULL, .file_count = 0};

    assert(html_render(&m, bad_path, "Should Fail") == -1);

    printf("test_write_failure passed\n");
}

int main(int argc, char **argv) {
    const char *out_dir = argc > 1 ? argv[1] : "tests/tmp";

    test_nested_dir_tree(out_dir);
    test_error_file(out_dir);
    test_full_symbol(out_dir);
    test_minimal_symbol(out_dir);
    test_default_title(out_dir);
    test_html_escaping(out_dir);
    test_malformed_parent_index(out_dir);
    test_diagram_without_refs(out_dir);
    test_refs_without_diagram(out_dir);
    test_unnamed_fallbacks(out_dir);
    test_sibling_order(out_dir);
    test_multiple_symbols_in_file(out_dir);
    test_write_failure(out_dir);

    printf("\nAll html_renderer checks passed.\n");
    return 0;
}