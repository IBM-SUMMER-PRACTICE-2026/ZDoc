/* Unit tests for the zdoc CLI front-end, in the same shape as the renderer
   tests: build small fixtures, call the unit directly, assert on the result.
   Covers options.c (defaults), cli.c (zd_cli_parse), config.c (zdoc.yaml /
   zdoc.json loading) and request.c (validate + the generate-request JSON).

   Checks are failure-tolerant: a failed CHECK reports file:line and lets
   every remaining test run, and main exits nonzero if anything failed.
   Plain assert() is kept only for harness plumbing (scratch files, chdir)
   whose failure means the test setup itself is broken.

   The error-path tests intentionally trigger zdoc's stderr diagnostics, so
   those lines are expected in the output. The --help/--version/about text
   is captured into the scratch dir instead of drowning the test output. */

/* -std=c11 is strict ANSI: glibc hides fileno/dup behind this macro, and
   MinGW hides _fileno entirely (declared by hand below). */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "../cli.h"
#include "../config.h"
#include "../options.h"
#include "../request.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
int _fileno(FILE *stream);
#define zd_mkdir(p) _mkdir(p)
#define zd_chdir(p) _chdir(p)
#define zd_getcwd(b, n) _getcwd(b, n)
#define zd_dup _dup
#define zd_dup2 _dup2
#define zd_close _close
#define zd_fileno _fileno
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define zd_mkdir(p) mkdir(p, 0777)
#define zd_chdir(p) chdir(p)
#define zd_getcwd(b, n) getcwd(b, n)
#define zd_dup dup
#define zd_dup2 dup2
#define zd_close close
#define zd_fileno fileno
#endif

static int g_run_failed;  /* failed checks across the whole run */
static int g_test_failed; /* failed checks in the test currently running */

/* Record a failed expectation without aborting, so the remaining tests
   still run. Where a later check would read memory that is only valid if
   an earlier one held (list counts vs. list contents), the conditions are
   combined into one CHECK with && so the short-circuit keeps it safe. */
#define CHECK(cond) do { \
    if(!(cond)) { \
        printf("  FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        g_test_failed++; \
    } \
} while(0)

//Per-test verdict, folding this test's failures into the run total
static void test_result(const char *name) {
    if(g_test_failed) {
        printf("%s FAILED (%d check%s)\n", name, g_test_failed,
               g_test_failed == 1 ? "" : "s");
        g_run_failed += g_test_failed;
        g_test_failed = 0;
    } else {
        printf("%s passed\n", name);
    }
}

/* Where the test binary started; config tests chdir away and back. */
static char g_start_dir[1024];

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

static void write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "wb");
    assert(f != NULL);
    fputs(content, f);
    fclose(f);
}

/* Parse with stdout redirected into path, so the help/about/version text
   lands in the scratch dir where the test can assert on it instead of
   flooding the test output. */
static zd_cli_result parse_capturing(int argc, char **argv, zd_options *o, const char *path) {
    int saved;
    zd_cli_result r;

    fflush(stdout);
    saved = zd_dup(zd_fileno(stdout));
    assert(saved >= 0);
    assert(freopen(path, "wb", stdout) != NULL);

    r = zd_cli_parse(argc, argv, o);

    fflush(stdout);
    zd_dup2(saved, zd_fileno(stdout));
    zd_close(saved);
    return r;
}

/* Run zd_config_load with dir as the working directory, since it only ever
   reads ./zdoc.yaml / ./zdoc.json. Restores the starting directory after. */
static void load_config_in(const char *dir, zd_options *o) {
    assert(zd_chdir(dir) == 0);
    zd_config_load(o);
    assert(zd_chdir(g_start_dir) == 0);
}

/* ---- options.c ---- */

//Built-in defaults: offline md into ./zdoc-out, bob on PATH, empty lists
static void test_options_init_defaults(void) {
    zd_options o;
    zd_options_init(&o);

    CHECK(o.mode == ZD_MODE_OFFLINE);
    CHECK(o.format == ZD_FORMAT_MD);
    CHECK(strcmp(o.out_dir, "./zdoc-out") == 0);
    CHECK(strcmp(o.bob_cli, "bob") == 0);
    CHECK(o.title[0] == '\0');
    CHECK(o.bob_args[0] == '\0');
    CHECK(o.languages != NULL); //list slots are heap-allocated up front
    CHECK(o.excludes != NULL);
    CHECK(o.n_languages == 0);
    CHECK(o.n_excludes == 0);
    CHECK(o.n_inputs == 0);
    CHECK(o.recursive == 0);
    CHECK(o.no_source == 0);

    test_result("test_options_init_defaults");
}

//Enum-to-name mapping used when the request JSON is written
static void test_mode_format_names(void) {
    CHECK(strcmp(zd_mode_name(ZD_MODE_OFFLINE), "offline") == 0);
    CHECK(strcmp(zd_mode_name(ZD_MODE_AI), "ai") == 0);
    CHECK(strcmp(zd_format_name(ZD_FORMAT_MD), "md") == 0);
    CHECK(strcmp(zd_format_name(ZD_FORMAT_HTML), "html") == 0);

    test_result("test_mode_format_names");
}

/* ---- cli.c ---- */

//Bare "zdoc", --help and --version all print and ask main to exit cleanly
static void test_cli_exit_paths(const char *scratch) {
    char path[600];
    snprintf(path, sizeof path, "%s/cli_stdout.txt", scratch);

    zd_options o;
    char *out;

    char *bare[] = { "zdoc" };
    zd_options_init(&o);
    CHECK(parse_capturing(1, bare, &o, path) == ZD_CLI_EXIT);
    out = read_file(path);
    CHECK(strstr(out, "usage:") != NULL);
    free(out);

    char *help[] = { "zdoc", "--help" };
    zd_options_init(&o);
    CHECK(parse_capturing(2, help, &o, path) == ZD_CLI_EXIT);
    out = read_file(path);
    CHECK(strstr(out, "--output-format md|html") != NULL);
    free(out);

    char *h[] = { "zdoc", "-h" };
    zd_options_init(&o);
    CHECK(parse_capturing(2, h, &o, path) == ZD_CLI_EXIT);

    char *version[] = { "zdoc", "--version" };
    zd_options_init(&o);
    CHECK(parse_capturing(2, version, &o, path) == ZD_CLI_EXIT);
    out = read_file(path);
    CHECK(strcmp(out, "zdoc " ZD_VERSION "\n") == 0);
    free(out);

    test_result("test_cli_exit_paths");
}

//A lone source path parses OK and leaves every default untouched
static void test_cli_input_only(void) {
    char *argv[] = { "zdoc", "./src" };
    zd_options o;
    zd_options_init(&o);

    CHECK(zd_cli_parse(2, argv, &o) == ZD_CLI_OK);
    CHECK(o.n_inputs == 1);
    CHECK(strcmp(o.inputs[0], "./src") == 0);
    CHECK(o.mode == ZD_MODE_OFFLINE);
    CHECK(o.format == ZD_FORMAT_MD);
    CHECK(strcmp(o.out_dir, "./zdoc-out") == 0);

    test_result("test_cli_input_only");
}

/*Every value option in both "--opt value" and "--opt=value" spelling,
plus the two flags; "-" counts as a source path, not an option.*/
static void test_cli_all_options(void) {
    char *argv[] = {
        "zdoc",
        "--mode", "ai",
        "--output-format=html",
        "--out-dir", "./docs",
        "--title=My Project",
        "--bob-cli", "/usr/local/bin/bob",
        "--bob-args=--fast --verbose",
        "--recursive",
        "--no-source",
        "-",
        "./src",
    };
    zd_options o;
    zd_options_init(&o);

    CHECK(zd_cli_parse(14, argv, &o) == ZD_CLI_OK);
    CHECK(o.mode == ZD_MODE_AI);
    CHECK(o.format == ZD_FORMAT_HTML);
    CHECK(strcmp(o.out_dir, "./docs") == 0);
    CHECK(strcmp(o.title, "My Project") == 0);
    CHECK(strcmp(o.bob_cli, "/usr/local/bin/bob") == 0);
    CHECK(strcmp(o.bob_args, "--fast --verbose") == 0);
    CHECK(o.recursive == 1);
    CHECK(o.no_source == 1);
    CHECK(o.n_inputs == 2);
    CHECK(strcmp(o.inputs[0], "-") == 0);
    CHECK(strcmp(o.inputs[1], "./src") == 0);

    test_result("test_cli_all_options");
}

/*--lang splits its comma list into extension tokens stored verbatim, and
the first --lang replaces whatever a config file put in the list instead
of appending.*/
static void test_cli_lang_list(void) {
    char *argv[] = { "zdoc", "--lang", ".c,.cpp, .java", "./src" };
    zd_options o;
    zd_options_init(&o);

    //pretend zdoc.yaml already picked a language
    snprintf(o.languages[0], ZD_LANG_MAX, ".plx");
    o.n_languages = 1;

    CHECK(zd_cli_parse(4, argv, &o) == ZD_CLI_OK);
    CHECK(o.n_languages == 3
          && strcmp(o.languages[0], ".c") == 0
          && strcmp(o.languages[1], ".cpp") == 0
          && strcmp(o.languages[2], ".java") == 0);

    test_result("test_cli_lang_list");
}

//--exclude repeats to collect globs, and like --lang replaces config lists
static void test_cli_exclude_repeatable(void) {
    char *argv[] = { "zdoc", "--exclude", "*.bak", "--exclude=build/*", "./src" };
    zd_options o;
    zd_options_init(&o);

    snprintf(o.excludes[0], ZD_GLOB_MAX, "from-config/*");
    o.n_excludes = 1;

    CHECK(zd_cli_parse(5, argv, &o) == ZD_CLI_OK);
    CHECK(o.n_excludes == 2
          && strcmp(o.excludes[0], "*.bak") == 0
          && strcmp(o.excludes[1], "build/*") == 0);

    test_result("test_cli_exclude_repeatable");
}

/*Bad enum values, unknown options, a value option left without a value
and a command line with no source path all fail the parse.*/
static void test_cli_errors(void) {
    zd_options o;

    char *bad_mode[] = { "zdoc", "--mode", "turbo", "./src" };
    zd_options_init(&o);
    CHECK(zd_cli_parse(4, bad_mode, &o) == ZD_CLI_ERROR);

    char *bad_format[] = { "zdoc", "--output-format=pdf", "./src" };
    zd_options_init(&o);
    CHECK(zd_cli_parse(3, bad_format, &o) == ZD_CLI_ERROR);

    char *unknown[] = { "zdoc", "--frobnicate", "./src" };
    zd_options_init(&o);
    CHECK(zd_cli_parse(3, unknown, &o) == ZD_CLI_ERROR);

    char *no_value[] = { "zdoc", "./src", "--title" };
    zd_options_init(&o);
    CHECK(zd_cli_parse(3, no_value, &o) == ZD_CLI_ERROR);

    char *no_input[] = { "zdoc", "--recursive" };
    zd_options_init(&o);
    CHECK(zd_cli_parse(2, no_input, &o) == ZD_CLI_ERROR);

    test_result("test_cli_errors");
}

/* ---- config.c ---- */

//No zdoc.yaml and no zdoc.json in the directory leaves the defaults alone
static void test_config_missing_is_noop(const char *scratch) {
    char dir[512];
    snprintf(dir, sizeof dir, "%s/cfg_none", scratch);
    zd_mkdir(dir);

    zd_options o;
    zd_options_init(&o);
    load_config_in(dir, &o);

    //still exactly the zd_options_init defaults
    CHECK(o.mode == ZD_MODE_OFFLINE);
    CHECK(o.format == ZD_FORMAT_MD);
    CHECK(strcmp(o.out_dir, "./zdoc-out") == 0);
    CHECK(strcmp(o.bob_cli, "bob") == 0);
    CHECK(o.title[0] == '\0');
    CHECK(o.n_languages == 0);
    CHECK(o.n_excludes == 0);
    CHECK(o.recursive == 0);
    CHECK(o.no_source == 0);

    test_result("test_config_missing_is_noop");
}

/*zdoc.yaml scalars and lists: comments are stripped, quotes removed,
booleans accept yes/true, list items are stored verbatim, and the list
keys replace the defaults. An unknown key only warns.*/
static void test_config_yaml(const char *scratch) {
    char dir[512], path[600];
    snprintf(dir, sizeof dir, "%s/cfg_yaml", scratch);
    zd_mkdir(dir);
    snprintf(path, sizeof path, "%s/zdoc.yaml", dir);

    write_file(path,
        "# project config\n"
        "title: \"My Project\"   # trailing comment\n"
        "out_dir: ./docs\n"
        "mode: ai\n"
        "output_format: html\n"
        "recursive: true\n"
        "no_source: yes\n"
        "bob_cli: /opt/bob\n"
        "bob_args: '--fast'\n"
        "languages:\n"
        "  - .cpp\n"
        "  - .java\n"
        "exclude:\n"
        "  - \"*.bak\"\n"
        "  - build/*\n"
        "unknown_key: whatever\n");

    zd_options o;
    zd_options_init(&o);
    load_config_in(dir, &o);

    CHECK(strcmp(o.title, "My Project") == 0);
    CHECK(strcmp(o.out_dir, "./docs") == 0);
    CHECK(o.mode == ZD_MODE_AI);
    CHECK(o.format == ZD_FORMAT_HTML);
    CHECK(o.recursive == 1);
    CHECK(o.no_source == 1);
    CHECK(strcmp(o.bob_cli, "/opt/bob") == 0);
    CHECK(strcmp(o.bob_args, "--fast") == 0);
    CHECK(o.n_languages == 2
          && strcmp(o.languages[0], ".cpp") == 0
          && strcmp(o.languages[1], ".java") == 0);
    CHECK(o.n_excludes == 2
          && strcmp(o.excludes[0], "*.bak") == 0
          && strcmp(o.excludes[1], "build/*") == 0);

    test_result("test_config_yaml");
}

/*zdoc.json is the fallback: strings with \\ escapes, booleans and string
lists all apply.*/
static void test_config_json(const char *scratch) {
    char dir[512], path[600];
    snprintf(dir, sizeof dir, "%s/cfg_json", scratch);
    zd_mkdir(dir);
    snprintf(path, sizeof path, "%s/zdoc.json", dir);

    write_file(path,
        "{\n"
        "  \"title\": \"Json \\\"Title\\\"\",\n"
        "  \"out_dir\": \"C:\\\\docs\",\n"
        "  \"recursive\": true,\n"
        "  \"languages\": [\".asm\", \".plx\"],\n"
        "  \"exclude\": [\"*.o\"]\n"
        "}\n");

    zd_options o;
    zd_options_init(&o);
    load_config_in(dir, &o);

    CHECK(strcmp(o.title, "Json \"Title\"") == 0);
    CHECK(strcmp(o.out_dir, "C:\\docs") == 0);
    CHECK(o.recursive == 1);
    CHECK(o.n_languages == 2
          && strcmp(o.languages[0], ".asm") == 0
          && strcmp(o.languages[1], ".plx") == 0);
    CHECK(o.n_excludes == 1 && strcmp(o.excludes[0], "*.o") == 0);

    test_result("test_config_json");
}

//When both files exist zdoc.yaml wins and zdoc.json is never read
static void test_config_yaml_beats_json(const char *scratch) {
    char dir[512], path[600];
    snprintf(dir, sizeof dir, "%s/cfg_both", scratch);
    zd_mkdir(dir);

    snprintf(path, sizeof path, "%s/zdoc.yaml", dir);
    write_file(path, "title: From Yaml\n");
    snprintf(path, sizeof path, "%s/zdoc.json", dir);
    write_file(path, "{ \"title\": \"From Json\" }\n");

    zd_options o;
    zd_options_init(&o);
    load_config_in(dir, &o);

    CHECK(strcmp(o.title, "From Yaml") == 0);

    test_result("test_config_yaml_beats_json");
}

/*The documented precedence chain: defaults -> zdoc.yaml -> command line.
The CLI overrides the file's scalars and replaces its language list.*/
static void test_precedence_cli_over_config(const char *scratch) {
    char dir[512], path[600];
    snprintf(dir, sizeof dir, "%s/cfg_prec", scratch);
    zd_mkdir(dir);
    snprintf(path, sizeof path, "%s/zdoc.yaml", dir);

    write_file(path,
        "title: Config Title\n"
        "out_dir: ./from-config\n"
        "languages:\n"
        "  - .c\n");

    zd_options o;
    zd_options_init(&o);
    load_config_in(dir, &o);

    char *argv[] = { "zdoc", "--title", "CLI Title", "--lang", ".plx", "./src" };
    CHECK(zd_cli_parse(6, argv, &o) == ZD_CLI_OK);

    CHECK(strcmp(o.title, "CLI Title") == 0);
    CHECK(strcmp(o.out_dir, "./from-config") == 0); //not overridden, file value stays
    CHECK(o.n_languages == 1 && strcmp(o.languages[0], ".plx") == 0);

    test_result("test_precedence_cli_over_config");
}

/* ---- request.c ---- */

//Validation passes when every source path exists and fails when one doesn't
static void test_request_validate(void) {
    zd_options o;
    zd_options_init(&o);

    snprintf(o.inputs[0], ZD_PATH_MAX, ".");
    o.n_inputs = 1;
    CHECK(zd_request_validate(&o) == 0);

    snprintf(o.inputs[1], ZD_PATH_MAX, "definitely-missing-dir-xyz");
    o.n_inputs = 2;
    CHECK(zd_request_validate(&o) == -1);

    test_result("test_request_validate");
}

/*The generate-request JSON carries every resolved option; backslashes in
paths and quotes in titles are escaped so the payload stays valid JSON.*/
static void test_request_write(const char *scratch) {
    char path[600];
    snprintf(path, sizeof path, "%s/request.json", scratch);

    zd_options o;
    zd_options_init(&o);
    o.mode = ZD_MODE_AI;
    o.format = ZD_FORMAT_HTML;
    snprintf(o.out_dir, ZD_PATH_MAX, "C:\\docs\\out");
    snprintf(o.title, ZD_TITLE_MAX, "My \"Project\"");
    o.recursive = 1;
    snprintf(o.languages[0], ZD_LANG_MAX, ".c");
    snprintf(o.languages[1], ZD_LANG_MAX, ".java");
    o.n_languages = 2;
    snprintf(o.excludes[0], ZD_GLOB_MAX, "*.bak");
    o.n_excludes = 1;
    snprintf(o.inputs[0], ZD_PATH_MAX, "./src");
    o.n_inputs = 1;

    FILE *f = fopen(path, "wb");
    assert(f != NULL);
    zd_request_write(&o, f);
    fclose(f);

    char *json = read_file(path);

    CHECK(strstr(json, "\"zdoc_request\": \"generate\"") != NULL);
    CHECK(strstr(json, "\"client_version\": \"" ZD_VERSION "\"") != NULL);
    CHECK(strstr(json, "\"mode\": \"ai\"") != NULL);
    CHECK(strstr(json, "\"output_format\": \"html\"") != NULL);
    CHECK(strstr(json, "\"out_dir\": \"C:\\\\docs\\\\out\"") != NULL);
    CHECK(strstr(json, "\"title\": \"My \\\"Project\\\"\"") != NULL);
    CHECK(strstr(json, "\"recursive\": true") != NULL);
    CHECK(strstr(json, "\"no_source\": false") != NULL);
    CHECK(strstr(json, "\"languages\": [\".c\", \".java\"]") != NULL);
    CHECK(strstr(json, "\"exclude\": [\"*.bak\"]") != NULL);
    CHECK(strstr(json, "\"bob_cli\": \"bob\"") != NULL);
    CHECK(strstr(json, "\"sources\": [\"./src\"]") != NULL);

    free(json);
    test_result("test_request_write");
}

int main(int argc, char **argv) {
    const char *scratch = argc > 1 ? argv[1] : "tests/tmp";

    assert(zd_getcwd(g_start_dir, sizeof g_start_dir) != NULL);
    zd_mkdir(scratch);

    test_options_init_defaults();
    test_mode_format_names();

    test_cli_exit_paths(scratch);
    test_cli_input_only();
    test_cli_all_options();
    test_cli_lang_list();
    test_cli_exclude_repeatable();
    test_cli_errors();

    test_config_missing_is_noop(scratch);
    test_config_yaml(scratch);
    test_config_json(scratch);
    test_config_yaml_beats_json(scratch);
    test_precedence_cli_over_config(scratch);

    test_request_validate();
    test_request_write(scratch);

    if(g_run_failed) {
        printf("\n%d zdoc CLI check%s FAILED.\n", g_run_failed,
               g_run_failed == 1 ? "" : "s");
        return -1;
    }
    printf("\nAll zdoc CLI checks passed.\n");
    return 0;
}
