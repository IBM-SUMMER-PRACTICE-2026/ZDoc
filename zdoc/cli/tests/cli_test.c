/* Unit tests for the zdoc CLI front-end, in the same shape as the renderer
   tests: build small fixtures, call the unit directly, assert on the result.
   Covers options.c (defaults + language table), cli.c (zd_cli_parse),
   config.c (zdoc.yaml / zdoc.json loading) and request.c (validate + the
   generate-request JSON).

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

    assert(o.mode == ZD_MODE_OFFLINE);
    assert(o.format == ZD_FORMAT_MD);
    assert(strcmp(o.out_dir, "./zdoc-out") == 0);
    assert(strcmp(o.bob_cli, "bob") == 0);
    assert(o.title[0] == '\0');
    assert(o.bob_args[0] == '\0');
    assert(o.n_languages == 0);
    assert(o.n_excludes == 0);
    assert(o.n_inputs == 0);
    assert(o.recursive == 0);
    assert(o.no_source == 0);

    printf("test_options_init_defaults passed\n");
}

/*The language table maps canonical names and aliases ("c++", "assembler")
to their canonical form case-insensitively; unknown names give NULL.*/
static void test_lang_canonical(void) {
    assert(strcmp(zd_lang_canonical("plx"), "plx") == 0);
    assert(strcmp(zd_lang_canonical("c"), "c") == 0);
    assert(strcmp(zd_lang_canonical("c++"), "cpp") == 0);
    assert(strcmp(zd_lang_canonical("assembler"), "asm") == 0);
    assert(strcmp(zd_lang_canonical("JAVA"), "java") == 0);
    assert(strcmp(zd_lang_canonical("Pascal"), "pascal") == 0);
    assert(zd_lang_canonical("cobol") == NULL);
    assert(zd_lang_canonical("") == NULL);

    printf("test_lang_canonical passed\n");
}

//Enum-to-name mapping used when the request JSON is written
static void test_mode_format_names(void) {
    assert(strcmp(zd_mode_name(ZD_MODE_OFFLINE), "offline") == 0);
    assert(strcmp(zd_mode_name(ZD_MODE_AI), "ai") == 0);
    assert(strcmp(zd_format_name(ZD_FORMAT_MD), "md") == 0);
    assert(strcmp(zd_format_name(ZD_FORMAT_HTML), "html") == 0);

    printf("test_mode_format_names passed\n");
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
    assert(parse_capturing(1, bare, &o, path) == ZD_CLI_EXIT);
    out = read_file(path);
    assert(strstr(out, "usage:") != NULL);
    free(out);

    char *help[] = { "zdoc", "--help" };
    zd_options_init(&o);
    assert(parse_capturing(2, help, &o, path) == ZD_CLI_EXIT);
    out = read_file(path);
    assert(strstr(out, "--output-format md|html") != NULL);
    free(out);

    char *h[] = { "zdoc", "-h" };
    zd_options_init(&o);
    assert(parse_capturing(2, h, &o, path) == ZD_CLI_EXIT);

    char *version[] = { "zdoc", "--version" };
    zd_options_init(&o);
    assert(parse_capturing(2, version, &o, path) == ZD_CLI_EXIT);
    out = read_file(path);
    assert(strcmp(out, "zdoc " ZD_VERSION "\n") == 0);
    free(out);

    printf("test_cli_exit_paths passed\n");
}

//A lone source path parses OK and leaves every default untouched
static void test_cli_input_only(void) {
    char *argv[] = { "zdoc", "./src" };
    zd_options o;
    zd_options_init(&o);

    assert(zd_cli_parse(2, argv, &o) == ZD_CLI_OK);
    assert(o.n_inputs == 1);
    assert(strcmp(o.inputs[0], "./src") == 0);
    assert(o.mode == ZD_MODE_OFFLINE);
    assert(o.format == ZD_FORMAT_MD);
    assert(strcmp(o.out_dir, "./zdoc-out") == 0);

    printf("test_cli_input_only passed\n");
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

    assert(zd_cli_parse(14, argv, &o) == ZD_CLI_OK);
    assert(o.mode == ZD_MODE_AI);
    assert(o.format == ZD_FORMAT_HTML);
    assert(strcmp(o.out_dir, "./docs") == 0);
    assert(strcmp(o.title, "My Project") == 0);
    assert(strcmp(o.bob_cli, "/usr/local/bin/bob") == 0);
    assert(strcmp(o.bob_args, "--fast --verbose") == 0);
    assert(o.recursive == 1);
    assert(o.no_source == 1);
    assert(o.n_inputs == 2);
    assert(strcmp(o.inputs[0], "-") == 0);
    assert(strcmp(o.inputs[1], "./src") == 0);

    printf("test_cli_all_options passed\n");
}

/*--lang splits its comma list, canonicalizes aliases, and the first --lang
replaces whatever a config file put in the list instead of appending.*/
static void test_cli_lang_list(void) {
    char *argv[] = { "zdoc", "--lang", "c++,JAVA, plx", "./src" };
    zd_options o;
    zd_options_init(&o);

    //pretend zdoc.yaml already picked a language
    snprintf(o.languages[0], ZD_LANG_MAX, "pascal");
    o.n_languages = 1;

    assert(zd_cli_parse(4, argv, &o) == ZD_CLI_OK);
    assert(o.n_languages == 3);
    assert(strcmp(o.languages[0], "cpp") == 0);
    assert(strcmp(o.languages[1], "java") == 0);
    assert(strcmp(o.languages[2], "plx") == 0);

    printf("test_cli_lang_list passed\n");
}

//--exclude repeats to collect globs, and like --lang replaces config lists
static void test_cli_exclude_repeatable(void) {
    char *argv[] = { "zdoc", "--exclude", "*.bak", "--exclude=build/*", "./src" };
    zd_options o;
    zd_options_init(&o);

    snprintf(o.excludes[0], ZD_GLOB_MAX, "from-config/*");
    o.n_excludes = 1;

    assert(zd_cli_parse(5, argv, &o) == ZD_CLI_OK);
    assert(o.n_excludes == 2);
    assert(strcmp(o.excludes[0], "*.bak") == 0);
    assert(strcmp(o.excludes[1], "build/*") == 0);

    printf("test_cli_exclude_repeatable passed\n");
}

/*Bad enum values, unknown languages, unknown options, a value option left
without a value and a command line with no source path all fail the parse.*/
static void test_cli_errors(void) {
    zd_options o;

    char *bad_mode[] = { "zdoc", "--mode", "turbo", "./src" };
    zd_options_init(&o);
    assert(zd_cli_parse(4, bad_mode, &o) == ZD_CLI_ERROR);

    char *bad_format[] = { "zdoc", "--output-format=pdf", "./src" };
    zd_options_init(&o);
    assert(zd_cli_parse(3, bad_format, &o) == ZD_CLI_ERROR);

    char *bad_lang[] = { "zdoc", "--lang", "cobol", "./src" };
    zd_options_init(&o);
    assert(zd_cli_parse(4, bad_lang, &o) == ZD_CLI_ERROR);

    char *unknown[] = { "zdoc", "--frobnicate", "./src" };
    zd_options_init(&o);
    assert(zd_cli_parse(3, unknown, &o) == ZD_CLI_ERROR);

    char *no_value[] = { "zdoc", "./src", "--title" };
    zd_options_init(&o);
    assert(zd_cli_parse(3, no_value, &o) == ZD_CLI_ERROR);

    char *no_input[] = { "zdoc", "--recursive" };
    zd_options_init(&o);
    assert(zd_cli_parse(2, no_input, &o) == ZD_CLI_ERROR);

    printf("test_cli_errors passed\n");
}

/* ---- config.c ---- */

//No zdoc.yaml and no zdoc.json in the directory leaves the defaults alone
static void test_config_missing_is_noop(const char *scratch) {
    char dir[512];
    snprintf(dir, sizeof dir, "%s/cfg_none", scratch);
    zd_mkdir(dir);

    zd_options o, defaults;
    zd_options_init(&o);
    zd_options_init(&defaults);

    load_config_in(dir, &o);
    assert(memcmp(&o, &defaults, sizeof o) == 0);

    printf("test_config_missing_is_noop passed\n");
}

/*zdoc.yaml scalars and lists: comments are stripped, quotes removed,
booleans accept yes/true, language aliases canonicalize, and the list
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
        "  - c++\n"
        "  - Java\n"
        "exclude:\n"
        "  - \"*.bak\"\n"
        "  - build/*\n"
        "unknown_key: whatever\n");

    zd_options o;
    zd_options_init(&o);
    load_config_in(dir, &o);

    assert(strcmp(o.title, "My Project") == 0);
    assert(strcmp(o.out_dir, "./docs") == 0);
    assert(o.mode == ZD_MODE_AI);
    assert(o.format == ZD_FORMAT_HTML);
    assert(o.recursive == 1);
    assert(o.no_source == 1);
    assert(strcmp(o.bob_cli, "/opt/bob") == 0);
    assert(strcmp(o.bob_args, "--fast") == 0);
    assert(o.n_languages == 2);
    assert(strcmp(o.languages[0], "cpp") == 0);
    assert(strcmp(o.languages[1], "java") == 0);
    assert(o.n_excludes == 2);
    assert(strcmp(o.excludes[0], "*.bak") == 0);
    assert(strcmp(o.excludes[1], "build/*") == 0);

    printf("test_config_yaml passed\n");
}

/*zdoc.json is the fallback: strings with \\ escapes, booleans and string
lists all apply; language aliases canonicalize here too.*/
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
        "  \"languages\": [\"assembler\", \"plx\"],\n"
        "  \"exclude\": [\"*.o\"]\n"
        "}\n");

    zd_options o;
    zd_options_init(&o);
    load_config_in(dir, &o);

    assert(strcmp(o.title, "Json \"Title\"") == 0);
    assert(strcmp(o.out_dir, "C:\\docs") == 0);
    assert(o.recursive == 1);
    assert(o.n_languages == 2);
    assert(strcmp(o.languages[0], "asm") == 0);
    assert(strcmp(o.languages[1], "plx") == 0);
    assert(o.n_excludes == 1);
    assert(strcmp(o.excludes[0], "*.o") == 0);

    printf("test_config_json passed\n");
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

    assert(strcmp(o.title, "From Yaml") == 0);

    printf("test_config_yaml_beats_json passed\n");
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
        "  - c\n");

    zd_options o;
    zd_options_init(&o);
    load_config_in(dir, &o);

    char *argv[] = { "zdoc", "--title", "CLI Title", "--lang", "plx", "./src" };
    assert(zd_cli_parse(6, argv, &o) == ZD_CLI_OK);

    assert(strcmp(o.title, "CLI Title") == 0);
    assert(strcmp(o.out_dir, "./from-config") == 0); //not overridden, file value stays
    assert(o.n_languages == 1);
    assert(strcmp(o.languages[0], "plx") == 0);

    printf("test_precedence_cli_over_config passed\n");
}

/* ---- request.c ---- */

//Validation passes when every source path exists and fails when one doesn't
static void test_request_validate(void) {
    zd_options o;
    zd_options_init(&o);

    snprintf(o.inputs[0], ZD_PATH_MAX, ".");
    o.n_inputs = 1;
    assert(zd_request_validate(&o) == 0);

    snprintf(o.inputs[1], ZD_PATH_MAX, "definitely-missing-dir-xyz");
    o.n_inputs = 2;
    assert(zd_request_validate(&o) == -1);

    printf("test_request_validate passed\n");
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
    snprintf(o.languages[0], ZD_LANG_MAX, "c");
    snprintf(o.languages[1], ZD_LANG_MAX, "java");
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

    assert(strstr(json, "\"zdoc_request\": \"generate\"") != NULL);
    assert(strstr(json, "\"client_version\": \"" ZD_VERSION "\"") != NULL);
    assert(strstr(json, "\"mode\": \"ai\"") != NULL);
    assert(strstr(json, "\"output_format\": \"html\"") != NULL);
    assert(strstr(json, "\"out_dir\": \"C:\\\\docs\\\\out\"") != NULL);
    assert(strstr(json, "\"title\": \"My \\\"Project\\\"\"") != NULL);
    assert(strstr(json, "\"recursive\": true") != NULL);
    assert(strstr(json, "\"no_source\": false") != NULL);
    assert(strstr(json, "\"languages\": [\"c\", \"java\"]") != NULL);
    assert(strstr(json, "\"exclude\": [\"*.bak\"]") != NULL);
    assert(strstr(json, "\"bob_cli\": \"bob\"") != NULL);
    assert(strstr(json, "\"sources\": [\"./src\"]") != NULL);

    free(json);
    printf("test_request_write passed\n");
}

int main(int argc, char **argv) {
    const char *scratch = argc > 1 ? argv[1] : "tests/tmp";

    assert(zd_getcwd(g_start_dir, sizeof g_start_dir) != NULL);
    zd_mkdir(scratch);

    test_options_init_defaults();
    test_lang_canonical();
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

    printf("\nAll zdoc CLI checks passed.\n");
    return 0;
}
