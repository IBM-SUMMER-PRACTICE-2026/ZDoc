/* Unit tests for the html renderer. 
  Renders a DxModel (built by the extractor) into out_dir/index.html
*/

#include "../html_renderer.h"

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


/*Read the contents of path into a NULL terminated heap buffer. 
Caller frees. Fails the test if the file cant be read.
*/
static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL)
        fail_test(__FILE__, __LINE__, "cannot open rendered output: %s", path);
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)len + 1);
    CHECK(buf != NULL);
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

    CHECK(html_render(&m, path, "Nested") == 0);

    char index[600];
    snprintf(index, sizeof index, "%s/index.html", path);
    char *html = read_file(index);

    CHECK_CONTAINS(html, "<summary>src/</summary>");
    CHECK_CONTAINS(html, "<summary>util/</summary>");
    CHECK_CONTAINS(html, "<summary>Helper.java</summary>");
    CHECK_CONTAINS(html, "No documented symbols");

    free(html);
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

    CHECK(html_render(&m, path, "Errors") == 0);

    char index[600];
    snprintf(index, sizeof index, "%s/index.html", path);
    char *html = read_file(index);

    CHECK_CONTAINS(html, "Parser failed for this file");
    CHECK_NOT_CONTAINS(html, "No documented symbols.");

    free(html);
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

    CHECK(html_render(&m, path, "Full Symbol") == 0);

    char index[600];
    snprintf(index, sizeof index, "%s/index.html", path);
    char *html = read_file(index);

    CHECK_CONTAINS(html, "id=\"sym-main\"");
    CHECK_CONTAINS(html, "<span class=\"brief\">\xe2\x80\x94 Program entry point</span>");
    CHECK_CONTAINS(html, "<code class=\"language-java\">public static void main(String[] args)</code>");
    CHECK_CONTAINS(html, "<td><code>args</code></td><td>command-line arguments</td>");
    CHECK_CONTAINS(html, "<p class=\"h\">Returns</p>\n<p>void</p>");
    CHECK_CONTAINS(html, "<p>Prints a greeting.</p>");
    CHECK_CONTAINS(html, "<pre class=\"mermaid\">flowchart TD Start --- End</pre>");
    CHECK_CONTAINS(html, "href=\"#sym-helper\"");
    CHECK_CONTAINS(html, "window.addEventListener('hashchange',reveal)");
    CHECK_CONTAINS(html, "cdn.jsdelivr.net/npm/mermaid");

    free(html);
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

    CHECK(html_render(&m,path, "Minimal Symbol") == 0);

    char index[600];
    snprintf(index, sizeof index, "%s/index.html", path);
    char *html = read_file(index);

    CHECK_CONTAINS(html, "id=\"sym-helper\"");
    CHECK_CONTAINS(html, "<code class=\"language-java\">private static void helper()</code>");
    CHECK_NOT_CONTAINS(html, "class=\"brief\"");
    CHECK_NOT_CONTAINS(html, "Parameters</p>");
    CHECK_NOT_CONTAINS(html, "Returns</p>");
    CHECK_NOT_CONTAINS(html, "Notes</p>");
    CHECK_NOT_CONTAINS(html, "Block Diagram</p>");
    CHECK_NOT_CONTAINS(html, "Cross-references</p>");
    CHECK_NOT_CONTAINS(html, "hashchange");
    CHECK_NOT_CONTAINS(html, "cdn.jsdelivr.net/npm/mermaid");
    CHECK_NOT_CONTAINS(html, "<pre class=\"mermaid\">");
    CHECK_NOT_CONTAINS(html, "pre.mermaid{display:none}");

    free(html);
}


//A NULL title falls back to the default "Documentation" heading.
static void test_default_title(const char *out_dir) {
    char path[512];
    snprintf(path, sizeof path, "%s/default_title", out_dir);

    DxDir dirs[] = { {.name = "src", .parent_index = -1 } };
    DxModel m = {.dirs = dirs, .dir_count = 1, .files = NULL, .file_count = 0};

    CHECK(html_render(&m, path, NULL) == 0);

    char index[600];
    snprintf(index, sizeof index, "%s/index.html", path); 
    char *html = read_file(index);

    CHECK_CONTAINS(html, "<title>Documentation</title>");
    CHECK_CONTAINS(html, "<h1>Documentation</h1>");

    free(html);
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

    CHECK(html_render(&m, path, "Escaping") == 0);

    char index[600];
    snprintf(index, sizeof index, "%s/index.html", path);
    char *html = read_file(index);

    CHECK_CONTAINS(html, "Weird&lt;File&gt;.c");
    CHECK_NOT_CONTAINS(html, "<summary>Weird<File>.c</summary>");
    CHECK_CONTAINS(html, "uses &lt;tags&gt; &amp; &quot;quotes&quot;");
    CHECK_CONTAINS(html, "&lt;script&gt;&quot;&amp;&quot;");
    CHECK_NOT_CONTAINS(html, "<script>\"&\"");

    free(html);
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

    CHECK(html_render(&m, path, "Malformed") == 0);

    char index[600];
    snprintf(index, sizeof index, "%s/index.html", path);
    char *html = read_file(index);

    CHECK_CONTAINS(html, "<summary>root_a/</summary>");
    CHECK_CONTAINS(html, "<summary>bad_parent/</summary>");
    CHECK_CONTAINS(html, "<summary>self_ref/</summary>");
    /* root_a has no children - if bad_parent/self_ref had been nested
       under it instead of promoted to root, this exact empty-body
       substring wouldn't appear. */
    CHECK_CONTAINS(html, "<summary>root_a/</summary><ul>\n</ul></details></li>");

    free(html);
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

    CHECK(html_render(&m, path, "Diagram Only") == 0);

    char index[600];
    snprintf(index, sizeof index, "%s/index.html", path);
    char *html = read_file(index);

    CHECK_CONTAINS(html, "cdn.jsdelivr.net/npm/mermaid");
    CHECK_CONTAINS(html, "pre.mermaid{display:none}");
    CHECK_NOT_CONTAINS(html, "hashchange");

    free(html);
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

    CHECK(html_render(&m, path, "Refs Only") == 0);

    char index[600];
    snprintf(index, sizeof index, "%s/index.html", path);
    char *html = read_file(index);

    CHECK_CONTAINS(html, "hashchange");
    CHECK_NOT_CONTAINS(html, "cdn.jsdelivr.net/npm/mermaid");
    CHECK_NOT_CONTAINS(html, "pre.mermaid{display:none}");

    free(html);
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

    CHECK(html_render(&m, path, "Unnamed") == 0);

    char index[600];
    snprintf(index, sizeof index, "%s/index.html", path);
    char *html = read_file(index);

    CHECK_CONTAINS(html, "<summary>(unnamed)/</summary>");
    CHECK_CONTAINS(html, "<summary>(unnamed)</summary>");
    CHECK_CONTAINS(html, "<details class=\"sym\"><summary><code>(unnamed)</code>");

    free(html);
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

    CHECK(html_render(&m, path, "Siblings") == 0);

    char index[600];
    snprintf(index, sizeof index, "%s/index.html", path);
    char *html = read_file(index);

    const char *a = strstr(html, "<summary>alpha/</summary>");
    const char *b = strstr(html, "<summary>beta/</summary>");
    const char *g = strstr(html, "<summary>gamma/</summary>");
    const char *one = strstr(html, "<summary>One.c</summary>");
    const char *two = strstr(html, "<summary>Two.c</summary>");
    CHECK(a && b && g && one && two);
    CHECK(a < b && b < g && g < one && one < two);

    free(html);
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

    CHECK(html_render(&m, path, "Multi Symbol") == 0);

    char index[600];
    snprintf(index, sizeof index, "%s/index.html", path);
    char *html = read_file(index);

    const char *first = strstr(html, "id=\"sym-first\"");
    const char *second = strstr(html, "id=\"sym-second\"");
    CHECK(first && second && first < second);

    free(html);
}

/*html_render fails cleanly (returns -1, doesn't crash) when it can't
create/open its output file - e.g. a path component collides with an
existing plain file. Relies on out_dir already existing from earlier
tests in this run.*/
static void test_write_failure(const char *out_dir) {
    char blocker_file[600];
    snprintf(blocker_file, sizeof blocker_file, "%s/blocker_write_failure", out_dir);
    FILE *f = fopen(blocker_file, "wb");
    CHECK(f != NULL);
    fclose(f);

    char bad_path[700];
    snprintf(bad_path, sizeof bad_path, "%s/sub", blocker_file);

    DxDir dirs[] = { {.name = "src", .parent_index = -1} };
    DxModel m = {.dirs = dirs, .dir_count = 1, .files = NULL, .file_count = 0};

    CHECK(html_render(&m, bad_path, "Should Fail") == -1);
}

/*A render that fails partway (a directory sits where index.html should
go, so the final rename fails) returns -1 and leaves no partial output
behind - in particular no index.html.tmp.*/
static void test_failed_render_leaves_no_output(const char *out_dir) {
    char path[512];
    snprintf(path, sizeof path, "%s/no_partial", out_dir);

    DxDir dirs[] = { {.name = "src", .parent_index = -1} };
    DxModel m = {.dirs = dirs, .dir_count = 1, .files = NULL, .file_count = 0};

    /* Rendering INTO <path>/index.html first leaves a non-empty directory
       with that name in the way of the real render below. */
    char blocker_dir[600];
    snprintf(blocker_dir, sizeof blocker_dir, "%s/index.html", path);
    CHECK(html_render(&m, blocker_dir, "Blocker") == 0);

    CHECK(html_render(&m, path, "Should Fail") == -1);

    char tmp_file[700];
    snprintf(tmp_file, sizeof tmp_file, "%s/index.html.tmp", path);
    FILE *f = fopen(tmp_file, "rb");
    CHECK(f == NULL);
}

int main(int argc, char **argv) {
    /* volatile: read again after a longjmp back into this frame */
    const char *volatile out_dir = argc > 1 ? argv[1] : "tests/tmp";

    RUN(test_nested_dir_tree(out_dir));
    RUN(test_error_file(out_dir));
    RUN(test_full_symbol(out_dir));
    RUN(test_minimal_symbol(out_dir));
    RUN(test_default_title(out_dir));
    RUN(test_html_escaping(out_dir));
    RUN(test_malformed_parent_index(out_dir));
    RUN(test_diagram_without_refs(out_dir));
    RUN(test_refs_without_diagram(out_dir));
    RUN(test_unnamed_fallbacks(out_dir));
    RUN(test_sibling_order(out_dir));
    RUN(test_multiple_symbols_in_file(out_dir));
    RUN(test_write_failure(out_dir));
    RUN(test_failed_render_leaves_no_output(out_dir));

    if (n_failed > 0) {
        printf("\n%d html_renderer test(s) FAILED.\n", n_failed);
        return 1;
    }
    printf("\nAll html_renderer checks passed.\n");
    return 0;
}