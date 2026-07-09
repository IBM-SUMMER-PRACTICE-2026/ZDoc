#include "cli.h"
#include "glob.h"
#include "../extractor/doc_extractor/module_tree/fs_walk.h"
#include "../extractor/doc_extractor/module_tree/modtree_tables.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
static int path_is_dir(const char *p) {
    DWORD a = GetFileAttributesA(p);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}
#else
#include <sys/stat.h>
static int path_is_dir(const char *p) {
    struct stat st;
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}
#endif

/* Extension filter passed to fs_walk: every extension of every selected
 * language (or of all languages when none were selected). */
static size_t build_ext_filter(const ZdocOptions *o, const char **out, size_t cap) {
    size_t n = 0;
    if (o->languages.count > 0) {
        for (size_t i = 0; i < o->languages.count; i++)
            zdoc_lang_extensions(o->languages.items[i], out, cap, &n);
    } else {
        for (size_t i = 0; i < zdoc_lang_count(); i++)
            zdoc_lang_extensions(zdoc_lang_name(i), out, cap, &n);
    }
    return n;
}

static int excluded(const ZdocOptions *o, const char *path) {
    for (size_t i = 0; i < o->exclude.count; i++)
        if (zdoc_glob_match(o->exclude.items[i], path)) return 1;
    return 0;
}

/* Dry run: discover the sources that the pipeline would process and print them.
 * The pipeline itself (parsers, extractor, renderer) is owned elsewhere and is
 * intentionally not invoked here. */
static void discover(const ZdocOptions *o) {
    const char *exts[64];
    size_t ext_n = build_ext_filter(o, exts, sizeof(exts) / sizeof(exts[0]));

    printf("\nDiscovered source files:\n");
    size_t total = 0;

    for (size_t si = 0; si < o->sources.count; si++) {
        const char *src = o->sources.items[si];

        if (!path_is_dir(src)) {
            /* Single file (or non-directory). */
            const char *lang = zdoc_lang_of_file(src);
            if (!lang) {
                fprintf(stderr, "zdoc: skipping '%s' (unsupported language)\n", src);
                continue;
            }
            if (excluded(o, src)) continue;
            printf("  [%-6s] %s\n", lang, src);
            total++;
            continue;
        }

        modtree_dir_table_t dirs;
        modtree_file_table_t files;
        modtree_dir_table_init(&dirs);
        modtree_file_table_init(&files);

        if (fs_walk(src, &dirs, &files, exts, ext_n) != 0) {
            fprintf(stderr, "zdoc: could not walk '%s'\n", src);
            modtree_dir_table_free(&dirs);
            modtree_file_table_free(&files);
            continue;
        }

        for (size_t k = 0; k < files.count; k++) {
            int pdir = files.files[k].parent_dir_index;
            /* --recursive off: keep only files sitting directly in a source
             * root (whose parent directory has no parent). */
            if (!o->recursive && dirs.dirs[pdir].parent_index != -1) continue;

            char path[4096];
            if (modtree_file_path(&dirs, &files, (int)k, path, sizeof(path)) != 0) continue;
            if (excluded(o, path)) continue;

            const char *lang = zdoc_lang_of_file(files.files[k].name);
            if (!lang) continue;

            printf("  [%-6s] %s\n", lang, path);
            total++;
        }

        modtree_dir_table_free(&dirs);
        modtree_file_table_free(&files);
    }

    printf("\nTotal: %zu file(s). (dry run - pipeline not yet wired in)\n", total);
}

int main(int argc, char **argv) {
    ZdocOptions o;
    zdoc_options_init(&o);

    /* Bare `zdoc` with no arguments prints usage (like `git`), exit 0. */
    if (argc < 2) {
        zdoc_print_usage(stdout, argc > 0 ? argv[0] : "zdoc");
        zdoc_options_free(&o);
        return 0;
    }

    if (zdoc_config_load("zdoc.yaml", &o) != 0) { zdoc_options_free(&o); return 2; }

    int r = zdoc_parse_args(argc, argv, &o);
    if (r == 1) { zdoc_options_free(&o); return 0; } /* --help / --version */
    if (r < 0) { zdoc_options_free(&o); return 2; }

    if (zdoc_options_validate(&o) != 0) { zdoc_options_free(&o); return 2; }

    zdoc_print_options(stdout, &o);
    discover(&o);

    zdoc_options_free(&o);
    return 0;
}
