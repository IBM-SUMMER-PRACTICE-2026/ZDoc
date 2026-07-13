/*
 * Walks the module_tree tables and a parsed Module array directly and
 * writes the result out as Markdown: one .md file per module (mirroring
 * the source directory structure) plus a root index.md linking to all of
 * them. No JSON, no parsing, no intermediate model - path reconstruction
 * uses module_tree's own modtree_file_path/modtree_dir_path instead of a
 * separate copy of that logic.
 */
#include "md_renderer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

/* Language is derived from the file's own extension, independently of
 * whether a parsed module matched it - a tiny local table, not tied to any
 * parser binary or source. Duplicated in html_renderer: both renderers
 * need this same small lookup, and there's no shared stage left to put it
 * in once, now that doc_extractor is gone. */
typedef struct { const char *ext; const char *language; } LangEntry;

static const LangEntry LANGUAGES[] = {
    { ".java", "java" },
    { ".c",    "c" },
    { ".h",    "c" },
    { ".cpp",  "cpp" },
    { ".cxx",  "cpp" },
    { ".cc",   "cpp" },
    { ".hpp",  "cpp" },
    { ".plx",  "plx" },
    { ".pls",  "plx" },
    { ".plas", "plas" },
};
#define LANGUAGE_COUNT (sizeof LANGUAGES / sizeof *LANGUAGES)

static const char *language_for_name(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if(!dot) return NULL;
    for(size_t i = 0; i < LANGUAGE_COUNT; i++)
        if(strcmp(dot, LANGUAGES[i].ext) == 0) return LANGUAGES[i].language;
    return NULL;
}

/* Maps file_index -> the module that parsed it, keyed by each module's own
 * pathIndex (the file table index the daemon stamped onto it while
 * parsing) rather than by assuming modules[i] corresponds to files[i]. That
 * assumption held only because of how the daemon currently happens to fill
 * the array; this index doesn't depend on modules being ordered, dense, or
 * the same length as files->count. Slots for files with no matching module
 * (parsing failed, was skipped, or hasn't run yet) stay NULL. */
static const Module **build_module_index(const Module *modules, size_t module_count, size_t file_count) {
    const Module **by_file = malloc(file_count * sizeof *by_file);
    if(!by_file) return NULL;
    for(size_t i = 0; i < file_count; i++) by_file[i] = NULL;
    for(size_t i = 0; i < module_count; i++) {
        int pi = modules[i].pathIndex;
        if(modules[i].filename && pi >= 0 && (size_t)pi < file_count)
            by_file[pi] = &modules[i];
    }
    return by_file;
}

/* Best-effort recursive mkdir - ignores failures (EEXIST is the common,
 * expected one); a genuine problem surfaces later when the file itself
 * fails to open for writing. */
static void mkdir_p(const char *dir) {
    char tmp[1024];
    snprintf(tmp, sizeof tmp, "%s", dir);
    size_t len = strlen(tmp);
    if(len == 0) return;
    if(tmp[len - 1] == '/') tmp[len - 1] = '\0';

    for(char *p = tmp + 1; *p; p++) {
        if(*p == '/') {
            *p = '\0';
            MKDIR(tmp);
            *p = '/';
        }
    }
    MKDIR(tmp);
}

/* Strips the extension off the filename component of an in-place path
 * (i.e. after the last '/', not any '.' that happens to be in a directory
 * name earlier in the path). */
static void strip_last_ext(char *path) {
    char *slash = strrchr(path, '/');
    char *base = slash ? slash + 1 : path;
    char *dot = strrchr(base, '.');
    if(dot) *dot = '\0';
}

/* Where a given file's rendered Markdown lives, relative to out_dir - the
 * source path with its extension swapped for .md. Used both as the actual
 * write location and as index.md's link target, so the two can never
 * disagree with each other. */
static int md_output_relpath(const modtree_dir_table_t *dirs, const modtree_file_table_t *files,
                              size_t file_index, char *out, size_t out_size) {
    char src_path[900];
    if(modtree_file_path(dirs, files, (int)file_index, src_path, sizeof src_path) != 0) return -1;
    strip_last_ext(src_path);
    int n = snprintf(out, out_size, "%s.md", src_path);
    return (n < 0 || (size_t)n >= out_size) ? -1 : 0;
}

static void write_param_table(FILE *o, const Symbol *s) {
    if(s->inputCount == 0) return;
    fputs("\n**Parameters**\n\n| Name | Description |\n|------|-------------|\n", o);
    for(int i = 0; i < s->inputCount; i++) {
        fprintf(o, "| %s | %s |\n",
                s->input[i].name ? s->input[i].name : "",
                s->input[i].description ? s->input[i].description : "");
    }
}

static void write_symbol(FILE *o, const Symbol *s, const char *language) {
    fputs("<details>\n<summary><strong>", o);
    fputs(s->name ? s->name : "(unnamed)", o);
    fputs("</strong>", o);
    if(s->description && s->description[0]) { fputs(" — ", o); fputs(s->description, o); }
    fputs("</summary>\n\n", o);

    fputs("**Signature**\n```", o);
    fputs(language ? language : "", o);
    fputc('\n', o);
    fputs(s->signature ? s->signature : "", o);
    fputs("\n```\n", o);

    write_param_table(o, s);

    if(s->output && s->output[0]) fprintf(o, "\n**Returns**\n%s\n", s->output);
    if(s->notes && s->notes[0]) fprintf(o, "\n**Notes**\n%s\n", s->notes);

    fputs("\n</details>\n\n", o);
}

static int write_module_file(const modtree_dir_table_t *dirs, const modtree_file_table_t *files,
                              const Module **by_file,
                              size_t file_index, const char *out_dir) {
    const modtree_file_t *f = &files->files[file_index];
    const char *language = f->name ? language_for_name(f->name) : NULL;
    const Module *mod = by_file[file_index];

    char relpath[900];
    if(md_output_relpath(dirs, files, file_index, relpath, sizeof relpath) != 0) return -1;

    char full_path[1200];
    snprintf(full_path, sizeof full_path, "%s/%s", out_dir, relpath);

    char dir_only[1200];
    snprintf(dir_only, sizeof dir_only, "%s", full_path);
    char *slash = strrchr(dir_only, '/');
    if(slash) { *slash = '\0'; mkdir_p(dir_only); }

    FILE *o = fopen(full_path, "wb");
    if(!o) return -1;

    char src_path[900];
    modtree_file_path(dirs, files, (int)file_index, src_path, sizeof src_path);
    fprintf(o, "# Module: %s\n\n", src_path);

    if(mod) {
        for(int k = 0; k < mod->symbolCount; k++) write_symbol(o, &mod->symbols[k], language);
    }

    fclose(o);
    return 0;
}

/* Recursively prints out_dir's directory/file structure as a nested,
 * linked bullet list - directories in bold, files linking to their
 * rendered .md page. dir_index == -1 is the (possibly virtual) root. */
static void print_tree(FILE *idx, const modtree_dir_table_t *dirs, const modtree_file_table_t *files,
                        int dir_index, int depth) {
    for(size_t i = 0; i < dirs->count; i++) {
        if(dirs->dirs[i].parent_index != dir_index) continue;
        for(int d = 0; d < depth; d++) fputs("  ", idx);
        fprintf(idx, "- **%s/**\n", dirs->dirs[i].name ? dirs->dirs[i].name : "");
        print_tree(idx, dirs, files, (int)i, depth + 1);
    }
    for(size_t i = 0; i < files->count; i++) {
        if(files->files[i].parent_dir_index != dir_index) continue;
        char relpath[900];
        if(md_output_relpath(dirs, files, i, relpath, sizeof relpath) != 0) continue;
        for(int d = 0; d < depth; d++) fputs("  ", idx);
        fprintf(idx, "- [%s](%s)\n", files->files[i].name ? files->files[i].name : "", relpath);
    }
}

static int write_index(const modtree_dir_table_t *dirs, const modtree_file_table_t *files,
                        const char *out_dir, const char *title) {
    char path[1200];
    snprintf(path, sizeof path, "%s/index.md", out_dir);

    FILE *o = fopen(path, "wb");
    if(!o) return -1;

    fprintf(o, "# %s\n\n", (title && title[0]) ? title : "Documentation");
    print_tree(o, dirs, files, -1, 0);

    fclose(o);
    return 0;
}

int md_render(const modtree_dir_table_t *dirs, const modtree_file_table_t *files,
              const Module *modules, size_t module_count,
              const char *out_dir, const char *title) {
    mkdir_p(out_dir);

    const Module **by_file = build_module_index(modules, module_count, files->count);
    if(!by_file) return -1;

    int rc = 0;
    for(size_t i = 0; i < files->count; i++) {
        if(write_module_file(dirs, files, by_file, i, out_dir) != 0) { rc = -1; break; }
    }

    free(by_file);
    if(rc != 0) return rc;
    return write_index(dirs, files, out_dir, title);
}
