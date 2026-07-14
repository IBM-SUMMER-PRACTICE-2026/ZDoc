/*
 * Walks a DxModel and writes it out as Markdown: one .md file per module
 * (mirroring the source directory structure) plus a root index.md linking
 * to all of them. No JSON, no parsing - see md_path.c for path
 * reconstruction, doc_extractor.h for the model itself.
 */
#include "md_renderer.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#define RMDIR(path) _rmdir(path)
#else
#include <sys/stat.h>
#include <unistd.h>
#define MKDIR(path) mkdir(path, 0755)
#define RMDIR(path) rmdir(path)
#endif

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
static int md_output_relpath(const DxModel *m, size_t file_index, char *out, size_t out_size) {
    char src_path[900];
    if(md_file_path(m, file_index, src_path, sizeof src_path) != 0) return -1;
    strip_last_ext(src_path);
    int n = snprintf(out, out_size, "%s.md", src_path);
    return (n < 0 || (size_t)n >= out_size) ? -1 : 0;
}

static void write_param_table(FILE *o, const DxSymbol *s) {
    if(s->param_count == 0) return;
    fputs("\n**Parameters**\n\n| Name | Description |\n|------|-------------|\n", o);
    for(size_t i = 0; i < s->param_count; i++) {
        fprintf(o, "| %s | %s |\n",
                s->params[i].name ? s->params[i].name : "",
                s->params[i].desc ? s->params[i].desc : "");
    }
}

static void write_symbol(FILE *o, const DxSymbol *s, const char *language) {
    fputs("<details>\n<summary><strong>", o);
    fputs(s->name ? s->name : "(unnamed)", o);
    fputs("</strong>", o);
    if(s->brief && s->brief[0]) { fputs(" — ", o); fputs(s->brief, o); }
    fputs("</summary>\n\n", o);

    fputs("**Signature**\n```", o);
    fputs(language ? language : "", o);
    fputc('\n', o);
    fputs(s->signature ? s->signature : "", o);
    fputs("\n```\n", o);

    write_param_table(o, s);

    if(s->returns && s->returns[0]) fprintf(o, "\n**Returns**\n%s\n", s->returns);
    if(s->notes && s->notes[0]) fprintf(o, "\n**Notes**\n%s\n", s->notes);

    fputs("\n</details>\n\n", o);
}

static int write_module_file(const DxModel *m, size_t file_index, const char *out_dir) {
    const DxFile *f = &m->files[file_index];

    char relpath[900];
    if(md_output_relpath(m, file_index, relpath, sizeof relpath) != 0) {
        fprintf(stderr, "md_renderer: output path too long for %s\n",
                f->name ? f->name : "(unnamed)");
        return -1;
    }

    char full_path[1200];
    snprintf(full_path, sizeof full_path, "%s/%s", out_dir, relpath);

    char dir_only[1200];
    snprintf(dir_only, sizeof dir_only, "%s", full_path);
    char *slash = strrchr(dir_only, '/');
    if(slash) { *slash = '\0'; mkdir_p(dir_only); }

    FILE *o = fopen(full_path, "wb");
    if(!o) {
        fprintf(stderr, "md_renderer: cannot open %s: %s\n",
                full_path, strerror(errno));
        return -1;
    }

    char src_path[900];
    md_file_path(m, file_index, src_path, sizeof src_path);
    fprintf(o, "# Module: %s\n\n", src_path);

    for(size_t k = 0; k < f->symbol_count; k++) write_symbol(o, &f->symbols[k], f->language);

    int rc = ferror(o) ? -1 : 0;
    if(fclose(o) != 0) rc = -1;
    if(rc != 0)
        fprintf(stderr, "md_renderer: write error on %s: %s\n",
                full_path, strerror(errno));
    return rc;
}

/* Recursively prints out_dir's directory/file structure as a nested,
 * linked bullet list - directories in bold, files linking to their
 * rendered .md page. dir_index == -1 is the (possibly virtual) root. */
static void print_tree(FILE *idx, const DxModel *m, int dir_index, int depth) {
    for(size_t i = 0; i < m->dir_count; i++) {
        if(m->dirs[i].parent_index != dir_index) continue;
        for(int d = 0; d < depth; d++) fputs("  ", idx);
        fprintf(idx, "- **%s/**\n", m->dirs[i].name ? m->dirs[i].name : "");
        print_tree(idx, m, (int)i, depth + 1);
    }
    for(size_t i = 0; i < m->file_count; i++) {
        if(m->files[i].parent_dir_index != dir_index) continue;
        char relpath[900];
        if(md_output_relpath(m, i, relpath, sizeof relpath) != 0) continue;
        for(int d = 0; d < depth; d++) fputs("  ", idx);
        fprintf(idx, "- [%s](%s)\n", m->files[i].name ? m->files[i].name : "", relpath);
    }
}

static int write_index(const DxModel *m, const char *out_dir, const char *title) {
    char path[1200];
    snprintf(path, sizeof path, "%s/index.md", out_dir);

    FILE *o = fopen(path, "wb");
    if(!o) {
        fprintf(stderr, "md_renderer: cannot open %s: %s\n",
                path, strerror(errno));
        return -1;
    }

    fprintf(o, "# %s\n\n", (title && title[0]) ? title : "Documentation");
    print_tree(o, m, -1, 0);

    int rc = ferror(o) ? -1 : 0;
    if(fclose(o) != 0) rc = -1;
    if(rc != 0)
        fprintf(stderr, "md_renderer: write error on %s: %s\n",
                path, strerror(errno));
    return rc;
}

/* Best-effort removal of everything a failed render wrote (module files up
 * to and including files_upto-1, index.md, and any directories that are now
 * empty), so the caller gets the error instead of a partial render. */
static void remove_outputs(const DxModel *m, size_t files_upto, const char *out_dir) {
    char full[1200];

    snprintf(full, sizeof full, "%s/index.md", out_dir);
    remove(full);

    for(size_t i = 0; i < files_upto; i++) {
        char relpath[900];
        if(md_output_relpath(m, i, relpath, sizeof relpath) != 0) continue;
        snprintf(full, sizeof full, "%s/%s", out_dir, relpath);
        remove(full);
    }

    /* Deepest directories tend to sit later in the table; walking it in
     * reverse removes children before parents. Non-empty ones (e.g. an
     * out_dir that existed before the render) are left alone. */
    for(size_t i = m->dir_count; i > 0; i--) {
        char relpath[900];
        if(md_dir_path(m, (int)(i - 1), relpath, sizeof relpath) != 0) continue;
        snprintf(full, sizeof full, "%s/%s", out_dir, relpath);
        RMDIR(full);
    }
    RMDIR(out_dir);
}

int md_render(const DxModel *m, const char *out_dir, const char *title) {
    mkdir_p(out_dir);
    for(size_t i = 0; i < m->file_count; i++) {
        if(write_module_file(m, i, out_dir) != 0) {
            remove_outputs(m, i + 1, out_dir); /* i + 1: drop the partial file too */
            return -1;
        }
    }
    if(write_index(m, out_dir, title) != 0) {
        remove_outputs(m, m->file_count, out_dir);
        return -1;
    }
    return 0;
}
