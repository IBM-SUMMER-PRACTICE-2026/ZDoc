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

/**
 * @brief Look up the display language for a source file by its extension.
 *
 * @param filename Name of the file being rendered (only the extension is
 *                 inspected).
 * @return The matching language tag from LANGUAGES, or NULL if filename has
 *         no extension or none match (the fenced code block then renders
 *         untagged).
 */
static const char *language_for_name(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if(!dot) return NULL;
    for(size_t i = 0; i < LANGUAGE_COUNT; i++)
        if(strcmp(dot, LANGUAGES[i].ext) == 0) return LANGUAGES[i].language;
    return NULL;
}

/**
 * @brief Build a file_index -> parsed Module lookup table.
 *
 * Keyed by each module's own pathIndex (the file table index the daemon
 * stamped onto it while parsing) rather than by assuming modules[i]
 * corresponds to files[i]. That assumption held only because of how the
 * daemon currently happens to fill the array; this index doesn't depend on
 * modules being ordered, dense, or the same length as files->count.
 *
 * @param modules Parsed module array.
 * @param module_count Number of entries in modules.
 * @param file_count Number of entries in the file table (size of the
 *                   returned array).
 * @return A newly heap-allocated array of file_count pointers (NULL on
 *         allocation failure); slots for files with no matching module
 *         (parsing failed, was skipped, or hasn't run yet) are NULL. Caller
 *         owns the array (not the Module pointers it contains).
 */
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

/**
 * @brief Recursively create a directory, best effort.
 *
 * Ignores failures (EEXIST is the common, expected one); a genuine problem
 * surfaces later when the file itself fails to open for writing.
 *
 * @param dir Directory path to create.
 */
static void mkdir_p(const char *dir) {
    char tmp[MD_PATH_MAX];
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

/**
 * @brief Compute a file's rendered Markdown path, relative to out_dir.
 *
 * The full source path with .md appended (not the source extension swapped
 * out: two files that differ only in extension, e.g. student_grades.plx and
 * student_grades.plxmac, would otherwise both reduce to student_grades.md
 * and silently overwrite one another). fs_walk never produces two files
 * with the same full relative path, so appending is guaranteed unique
 * where swapping wasn't. Used both as the actual write location and as
 * index.md's link target, so the two can never disagree.
 *
 * @param dirs Directory table.
 * @param files File table.
 * @param file_index Index of the file in files.
 * @param out Output buffer for the relative path.
 * @param out_size Size of out in bytes.
 * @return ZDOC_OK on success, or ZDOC_PATH_TOO_LONG if out was too small
 *         (propagated from modtree_file_path, or from the .md suffix
 *         itself overflowing).
 */
static enum ZDoc_Error md_output_relpath(const modtree_dir_table_t *dirs, const modtree_file_table_t *files,
                              size_t file_index, char *out, size_t out_size) {
    char src_path[MD_PATH_MAX];
    enum ZDoc_Error status = modtree_file_path(dirs, files, (int)file_index, src_path, sizeof src_path);
    if(status != ZDOC_OK) return status;
    int n = snprintf(out, out_size, "%s.md", src_path);
    return (n < 0 || (size_t)n >= out_size) ? ZDOC_PATH_TOO_LONG : ZDOC_OK;
}

/**
 * @brief Write a symbol's "Parameters" table.
 *
 * @param o Output stream.
 * @param s Symbol whose s->input entries should be listed.
 * @note Emits nothing when s->inputCount is 0, so a parameterless symbol's
 *       page doesn't carry an empty table.
 */
static void write_param_table(FILE *o, const Symbol *s) {
    if(s->inputCount == 0) return;
    fputs("\n**Parameters**\n\n| Name | Description |\n|------|-------------|\n", o);
    for(int i = 0; i < s->inputCount; i++) {
        fprintf(o, "| %s | %s |\n",
                s->input[i].name ? s->input[i].name : "",
                s->input[i].description ? s->input[i].description : "");
    }
}

/**
 * @brief Render a single symbol's documentation section.
 *
 * Emits a collapsible <details> block covering signature, kind, line,
 * parameters, returns, and notes (only the fields that are actually
 * present on s).
 *
 * @param o Output stream.
 * @param s Symbol to render.
 * @param language Language tag used to fence the signature code block
 *                 (may be NULL).
 */
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

    if(s->type && s->type[0]) fprintf(o, "\n**Kind**\n%s\n", s->type);
    fprintf(o, "\n**Line**\n%u\n", s->line);

    write_param_table(o, s);

    if(s->output && s->output[0]) fprintf(o, "\n**Returns**\n%s\n", s->output);
    if(s->notes && s->notes[0]) fprintf(o, "\n**Notes**\n%s\n", s->notes);
    if(s->diagram && s->diagram[0]) fprintf(o, "\n**Block Diagram**\n```mermaid\n%s\n```\n", s->diagram);

    fputs("\n</details>\n\n", o);
}

/**
 * @brief Check whether status is a recognized ZDoc_Error value.
 *
 * @param status Status code to validate.
 * @return Nonzero if status falls within the contiguous enum ZDoc_Error
 *         range, zero if it's garbage (e.g. an uninitialized Module.status).
 * @note Duplicated in html_renderer for the same reason as
 *       language_for_name - no shared stage left to hold one copy.
 */
static int zdoc_status_is_valid(enum ZDoc_Error status) {
    return status >= ZDOC_UNSUPPORTED_LANGUAGE && status <= ZDOC_UNSUPPORTED_FORMAT;
}

/**
 * @brief Write one file's own out_dir/<relpath>.md.
 *
 * Emits a "# Module: <path>" heading plus that file's symbols, or an error
 * notice (with the recorded status code, if any) when no module could be
 * matched to the file. Mirrors html_renderer's write_file_page - same
 * relpath scheme, same mkdir-of-parent-then-fopen shape.
 *
 * @param dirs Directory table.
 * @param files File table.
 * @param by_file file_index -> matched Module lookup, from
 *                build_module_index.
 * @param modules Raw parsed module array (used to recover a failed file's
 *                status when by_file has no match for it).
 * @param module_count Number of entries in modules.
 * @param file_index Index of the file being rendered.
 * @param out_dir Root output directory.
 * @return ZDOC_OK on success, ZDOC_PATH_TOO_LONG if the output path
 *         overflowed, or ZDOC_FILE_WRITE_FAILED if the page could not be
 *         opened, written, or closed.
 */
static enum ZDoc_Error write_module_file(const modtree_dir_table_t *dirs, const modtree_file_table_t *files,
                              const Module **by_file, const Module *modules, size_t module_count,
                              size_t file_index, const char *out_dir) {
    const modtree_file_t *f = &files->files[file_index];
    const char *language = f->name ? language_for_name(f->name) : NULL;
    const Module *mod = by_file[file_index];

    char relpath[MD_PATH_MAX];
    enum ZDoc_Error relpath_status = md_output_relpath(dirs, files, file_index, relpath, sizeof relpath);
    if(relpath_status != ZDOC_OK) return relpath_status;

    char full_path[MD_PATH_MAX];
    snprintf(full_path, sizeof full_path, "%s/%s", out_dir, relpath);

    char dir_only[MD_PATH_MAX];
    snprintf(dir_only, sizeof dir_only, "%s", full_path);
    char *slash = strrchr(dir_only, '/');
    if(slash) { *slash = '\0'; mkdir_p(dir_only); }

    FILE *o = fopen(full_path, "wb");
    if(!o) return ZDOC_FILE_WRITE_FAILED;

    char src_path[MD_PATH_MAX];
    modtree_file_path(dirs, files, (int)file_index, src_path, sizeof src_path);
    fprintf(o, "# Module: %s\n\n", src_path);

    if(!mod) {
        enum ZDoc_Error file_status = (file_index < module_count) ? modules[file_index].status : ZDOC_DEFAULT;
        fputs("\n*Parser failed for this file — no documentation extracted", o);
        if(file_status != ZDOC_OK && zdoc_status_is_valid(file_status))
            fprintf(o, " (error code %d)", (int)file_status);
        fputs(".*\n", o);
    } else if(mod->symbolCount == 0)
        fputs("\n*No documented symbols.*\n", o);
    else
        for(int k = 0; k < mod->symbolCount; k++) write_symbol(o, &mod->symbols[k], language);

    enum ZDoc_Error rc = ferror(o) ? ZDOC_FILE_WRITE_FAILED : ZDOC_OK;
    if(fclose(o) != 0) rc = ZDOC_FILE_WRITE_FAILED;
    return rc;
}

/**
 * @brief Recursively print out_dir's directory/file structure.
 *
 * Renders a nested, linked bullet list - directories in bold, files
 * linking to their rendered .md page.
 *
 * @param idx Output stream (the index.md being written).
 * @param dirs Directory table.
 * @param files File table.
 * @param dir_index Directory to print (-1 is the possibly-virtual root).
 * @param depth Current nesting depth, used for indentation.
 */
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
        char relpath[MD_PATH_MAX];
        if(md_output_relpath(dirs, files, i, relpath, sizeof relpath) != ZDOC_OK) continue;
        for(int d = 0; d < depth; d++) fputs("  ", idx);
        fprintf(idx, "- [%s](%s)\n", files->files[i].name ? files->files[i].name : "", relpath);
    }
}

/**
 * @brief Write out_dir/index.md.
 *
 * Emits a title heading plus the nested directory/file tree, files linking
 * out to their own rendered pages.
 *
 * @param dirs Directory table.
 * @param files File table.
 * @param out_dir Root output directory.
 * @param title Page title (falls back to "Documentation" if NULL/empty).
 * @return ZDOC_OK on success, or ZDOC_FILE_WRITE_FAILED if index.md could
 *         not be opened, written, or closed.
 */
static enum ZDoc_Error write_index(const modtree_dir_table_t *dirs, const modtree_file_table_t *files,
                        const char *out_dir, const char *title) {
    char path[MD_PATH_MAX];
    snprintf(path, sizeof path, "%s/index.md", out_dir);

    FILE *o = fopen(path, "wb");
    if(!o) return ZDOC_FILE_WRITE_FAILED;

    fprintf(o, "# %s\n\n", (title && title[0]) ? title : "Documentation");
    print_tree(o, dirs, files, -1, 0);

    enum ZDoc_Error rc = ferror(o) ? ZDOC_FILE_WRITE_FAILED : ZDOC_OK;
    if(fclose(o) != 0) rc = ZDOC_FILE_WRITE_FAILED;
    return rc;
}

/**
 * @brief Render the whole tree as Markdown.
 *
 * Produces one out_dir/index.md plus one out_dir/<relpath>.md per file -
 * the Markdown counterpart to html_render.
 *
 * @param dirs Directory table.
 * @param files File table.
 * @param modules Parsed module array.
 * @param module_count Number of entries in modules.
 * @param out_dir Root output directory (created if missing).
 * @param title Title used for index.md's heading (may be NULL).
 * @return ZDOC_OK on success, ZDOC_OUT_OF_MEMORY if the module index could
 *         not be allocated, or the first ZDoc_Error hit while writing a
 *         page or the index.
 */
enum ZDoc_Error md_render(const modtree_dir_table_t *dirs, const modtree_file_table_t *files,
              const Module *modules, size_t module_count,
              const char *out_dir, const char *title) {
    mkdir_p(out_dir);

    const Module **by_file = build_module_index(modules, module_count, files->count);
    if(!by_file) return ZDOC_OUT_OF_MEMORY;

    enum ZDoc_Error rc = ZDOC_OK;
    for(size_t i = 0; i < files->count; i++) {
        rc = write_module_file(dirs, files, by_file, modules, module_count, i, out_dir);
        if(rc != ZDOC_OK) break;
    }

    free(by_file);
    if(rc != ZDOC_OK) return rc;
    return write_index(dirs, files, out_dir, title);
}
