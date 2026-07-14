/*
  Walks the module_tree tables and a parsed Module array directly and
  writes the result out as one HTML page per module (mirroring the source
  directory structure, like md_renderer) plus a root index.html linking to
  all of them - each page has per-symbol documentation sections (signature,
  brief, parameters, returns, notes, block diagram). No JSON, no parsing, no
  intermediate model - matching a file to its parsed module and deriving
  its language happens right here instead of in a separate doc_extractor
  stage.
 */
#include "html_renderer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
/* Windows callers pass backslash paths (PowerShell tab completion emits
  them), and CreateDirectory accepts either separator. */
#define IS_SEP(c) ((c) == '/' || (c) == '\\')
#else
#include <sys/stat.h>
#define MKDIR(path) mkdir(path, 0755)
#define IS_SEP(c) ((c) == '/')
#endif

/* Language is derived from the file's own extension, independently of
 * whether a parsed module matched it - a tiny local table, not tied to any
 * parser binary or source. Duplicated in md_renderer: both renderers need
 * this same small lookup, and there's no shared stage left to put it in
 * once, now that doc_extractor is gone. */
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
    const Module **by_file = xmalloc(file_count * sizeof *by_file);
    for(size_t i = 0; i < file_count; i++) by_file[i] = NULL;
    for(size_t i = 0; i < module_count; i++) {
        int pi = modules[i].pathIndex;
        if(modules[i].filename && pi >= 0 && (size_t)pi < file_count)
            by_file[pi] = &modules[i];
    }
    return by_file;
}

/* Where a given file's rendered page lives, relative to out_dir - the full
 * source path with .html appended (not the source extension swapped out:
 * two files that differ only in extension, e.g. student_grades.plx and
 * student_grades.plxmac, would otherwise both reduce to
 * student_grades.html and silently overwrite one another). fs_walk never
 * produces two files with the same full relative path, so appending is
 * guaranteed unique where swapping wasn't. Used both as the actual write
 * location and as index.html's link target, so the two can never disagree
 * with each other. */
static int html_output_relpath(const modtree_dir_table_t *dirs, const modtree_file_table_t *files,
                                size_t file_index, char *out, size_t out_size) {
    char src_path[900];
    if(modtree_file_path(dirs, files, (int)file_index, src_path, sizeof src_path) != 0) return -1;
    int n = snprintf(out, out_size, "%s.html", src_path);
    return (n < 0 || (size_t)n >= out_size) ? -1 : 0;
}

/* Best effort recursive mkdir - ignores failures (EEXIST is the common,
  expected one); a genuine problem surfaces later when index.html itself
  fails to open for writing. */
static void mkdir_p(const char *dir) {
    char tmp[1024];
    snprintf(tmp, sizeof tmp, "%s", dir);
    size_t len = strlen(tmp);
    if(len == 0) return;
    if(IS_SEP(tmp[len - 1])) tmp[len - 1] = '\0';

    for(char *p = tmp + 1; *p; p++) {
        if(IS_SEP(*p)) {
            char sep = *p;
            *p = '\0';
            MKDIR(tmp);
            *p = sep;
        }
    }
    MKDIR(tmp);
}

/* HTML-escape s (NULL is rendered as nothing). Every model string goes
  through here - names, briefs, signatures and descriptions are arbitrary
  source text. */
static void put_escaped(FILE *out, const char *s) {
    if(!s) return;
    for(; *s; s++) {
        switch(*s) {
        case '&': fputs("&amp;", out);  break;
        case '<': fputs("&lt;", out);   break;
        case '>': fputs("&gt;", out);   break;
        case '"': fputs("&quot;", out); break;
        default:  fputc(*s, out);
        }
    }
}

/* Child lists threaded through the tree's parent links, so rendering can
  walk it top-down without rescanning both tables at every node:
    dir_child[d]  - first child directory of d, -1 if none
    dir_sib[d]    - next sibling directory of d
    file_child[d] - first file in directory d
    file_sib[f]   - next file in the same directory
    dir_root / file_root - heads of the top-level (parent == -1) lists
 */
typedef struct {
    int *dir_child, *dir_sib, *file_child, *file_sib;
    int dir_root, file_root;
} adjacency_t;
//Builds the adjacency table from the tables' parent links. The table is stack-allocated by the caller, and its arrays are heap-allocated here.
static void adjacency_build(adjacency_t *a, const modtree_dir_table_t *dirs,
                             const modtree_file_table_t *files) {
    a->dir_child  = xmalloc(dirs->count * sizeof(int));
    a->dir_sib    = xmalloc(dirs->count * sizeof(int));
    a->file_child = xmalloc(dirs->count * sizeof(int));
    a->file_sib   = xmalloc(files->count * sizeof(int));
    a->dir_root = a->file_root = -1;

    for(size_t i = 0; i < dirs->count; i++) {
        a->dir_child[i] = -1;
        a->file_child[i] = -1;
    }

    /* Reverse passes, so each child list comes out in table order. An
     * out-of-range or self-referencing parent index is demoted to a root
     * instead of being trusted. */
    for(size_t i = dirs->count; i-- > 0;) {
        int p = dirs->dirs[i].parent_index;
        if(p < 0 || (size_t)p >= dirs->count || (size_t)p == i) {
            a->dir_sib[i] = a->dir_root;
            a->dir_root = (int)i;
        } else {
            a->dir_sib[i] = a->dir_child[p];
            a->dir_child[p] = (int)i;
        }
    }

    for(size_t i = files->count; i-- > 0;) {
        int p = files->files[i].parent_dir_index;
        if(p < 0 || (size_t)p >= dirs->count) {
            a->file_sib[i] = a->file_root;
            a->file_root = (int)i;
        } else {
            a->file_sib[i] = a->file_child[p];
            a->file_child[p] = (int)i;
        }
    }
}
//Frees the adjacency table's arrays (the table itself is stack-allocated).
static void adjacency_free(adjacency_t *a) {
    free(a->dir_child);
    free(a->dir_sib);
    free(a->file_child);
    free(a->file_sib);
}

/*Renders a single symbol's documentation section.
The caller is responsible for wrapping the top-level call in <ul>...</ul> and for the adjacency table.*/
static void render_symbol(FILE *o, const Symbol *s, const char *language) {
    fputs("<details class=\"sym\"", o);
    if(s->name && s->name[0]) {
        /* Anchor target - kept even without cross-reference links, so a
          direct #sym-NAME URL still works. */
        fputs(" id=\"sym-", o);
        put_escaped(o, s->name);
        fputc('"', o);
    }
    fputs("><summary><code>", o);
    put_escaped(o, s->name ? s->name : "(unnamed)");
    fputs("</code>", o);
    if(s->description && s->description[0]) {
        fputs(" <span class=\"brief\">— ", o);
        put_escaped(o, s->description);
        fputs("</span>", o);
    }
    fputs("</summary>\n", o);

    fputs("<p class=\"h\">Signature</p>\n<pre><code", o);
    if(language && language[0]) {
        fputs(" class=\"language-", o);
        put_escaped(o, language);
        fputc('"', o);
    }
    fputc('>', o);
    put_escaped(o, s->signature);
    fputs("</code></pre>\n", o);

    if(s->type && s->type[0]) {
        fputs("<p class=\"h\">Kind</p>\n<p>", o);
        put_escaped(o, s->type);
        fputs("</p>\n", o);
    }
    fprintf(o, "<p class=\"h\">Line</p>\n<p>%u</p>\n", s->line);

    if(s->inputCount > 0) {
        fputs("<p class=\"h\">Parameters</p>\n"
              "<table><tr><th>Name</th><th>Description</th></tr>\n", o);
        for(int i = 0; i < s->inputCount; i++) {
            fputs("<tr><td><code>", o);
            put_escaped(o, s->input[i].name);
            fputs("</code></td><td>", o);
            put_escaped(o, s->input[i].description);
            fputs("</td></tr>\n", o);
        }
        fputs("</table>\n", o);
    }

    if(s->output && s->output[0]) {
        fputs("<p class=\"h\">Returns</p>\n<p>", o);
        put_escaped(o, s->output);
        fputs("</p>\n", o);
    }
    if(s->notes && s->notes[0]) {
        fputs("<p class=\"h\">Notes</p>\n<p>", o);
        put_escaped(o, s->notes);
        fputs("</p>\n", o);
    }
    if(s->diagram && s->diagram[0]) {
        /* Mermaid renders <pre class="mermaid"> in place; without JS the
          global <noscript> rules hide it and show this note instead (see
          docs/ZDOC.md -> Limitations). */
        fputs("<p class=\"h\">Block Diagram</p>\n"
              "<noscript><p class=\"empty\">Block diagram omitted — requires JavaScript.</p></noscript>\n"
              "<pre class=\"mermaid\">", o);
        put_escaped(o, s->diagram);
        fputs("</pre>\n", o);
    }
    fputs("</details>\n", o);
}
/*Renders the tree page as a nested, linked bullet list - directories in
 bold, files linking to their own rendered page. Mirrors md_renderer's
 print_tree, walked here via the adjacency table instead of a rescan per
 node.*/
static void render_index_tree(FILE *idx, const adjacency_t *a, const modtree_dir_table_t *dirs,
                               const modtree_file_table_t *files, int d) {
    fputs("<li><details class=\"dir\" open><summary>", idx);
    put_escaped(idx, dirs->dirs[d].name ? dirs->dirs[d].name : "(unnamed)");
    fputs("/</summary><ul>\n", idx);

    for(int c = a->dir_child[d]; c != -1; c = a->dir_sib[c])
        render_index_tree(idx, a, dirs, files, c);
    for(int f = a->file_child[d]; f != -1; f = a->file_sib[f]) {
        char relpath[900];
        if(html_output_relpath(dirs, files, (size_t)f, relpath, sizeof relpath) != 0) continue;
        fputs("<li><a href=\"", idx);
        put_escaped(idx, relpath);
        fputs("\">", idx);
        put_escaped(idx, files->files[f].name ? files->files[f].name : "(unnamed)");
        fputs("</a></li>\n", idx);
    }

    fputs("</ul></details></li>\n", idx);
}

// Embedded CSS for the single index.html output. No external dependencies.
static const char CSS[] =
    "body{font-family:ui-monospace,Consolas,monospace;margin:2rem;color:#222;max-width:60rem}\n"
    "h1{font-size:1.4rem;border-bottom:1px solid #ccc;padding-bottom:.4rem}\n"
    "ul{list-style:none;padding-left:1.25rem}\n"
    "ul.tree{padding-left:0}\n"
    "summary{cursor:pointer}\n"
    "details.dir>summary{font-weight:bold}\n"
    "details.file{margin:.15rem 0}\n"
    "details.sym{margin:.4rem 0 .4rem 1.25rem;padding:.1rem .6rem;border-left:2px solid #ddd}\n"
    ".brief{color:#555}\n"
    "p{margin:.2rem 0}\n"
    "p.h{font-weight:bold;margin:.6rem 0 .2rem}\n"
    "p.empty{color:#888;font-style:italic;margin-left:1.25rem}\n"
    "p.error{color:#a33;font-style:italic;margin-left:1.25rem}\n"
    "pre{background:#f6f6f6;padding:.5rem;overflow-x:auto;margin:.2rem 0}\n"
    "table{border-collapse:collapse;margin:.2rem 0}\n"
    "th,td{border:1px solid #ccc;padding:.25rem .6rem;text-align:left}\n"
    "th{background:#f6f6f6}\n"
    "pre.mermaid{background:#fff}\n";

/* Mermaid runs per-diagram when its enclosing <details> is opened - rendering
  inside a closed (hidden) node would come out zero-sized. AI Assisted mode
  already requires network access (docs/ZDOC.md -> Limitations), so the CDN
  load only happens for models that carry diagrams; offline output stays
  dependency-free. */
static const char MERMAID_JS[] =
    "import mermaid from 'https://cdn.jsdelivr.net/npm/mermaid@11/dist/mermaid.esm.min.mjs';\n"
    "mermaid.initialize({startOnLoad:false});\n"
    "function run(root){mermaid.run({nodes:root.querySelectorAll('pre.mermaid:not([data-processed])')});}\n"
    "document.querySelectorAll('details').forEach(function(d){\n"
    "d.addEventListener('toggle',function(){if(d.open)run(d);});});\n"
    "run(document);\n";

/* Opens the <head> for a page - title, embedded CSS, and (if the page
 * carries at least one diagram) the <noscript> fallback rule that hides
 * mermaid blocks when JS is off. Every emitted page shares this shell so
 * they stay visually consistent despite now being separate files. */
static void write_head(FILE *o, const char *title, int has_diagram) {
    fputs("<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"utf-8\">\n<title>", o);
    put_escaped(o, title);
    fputs("</title>\n<style>\n", o);
    fputs(CSS, o);
    fputs("</style>\n", o);
    if(has_diagram)
        fputs("<noscript><style>pre.mermaid{display:none}</style></noscript>\n", o);
    fputs("</head>\n<body>\n<h1>", o);
    put_escaped(o, title);
    fputs("</h1>\n", o);
}

static void write_foot(FILE *o, int has_diagram) {
    if(has_diagram) {
        fputs("<script type=\"module\">\n", o);
        fputs(MERMAID_JS, o);
        fputs("</script>\n", o);
    }
    fputs("</body>\n</html>\n", o);
}

/* Writes one file's own out_dir/<relpath>.html: page shell plus that
 * file's symbols. Mirrors md_renderer's write_module_file - same relpath
 * scheme, same mkdir-of-parent-then-fopen shape. */
static int write_file_page(const modtree_dir_table_t *dirs, const modtree_file_table_t *files,
                            const Module **by_file, size_t file_index, const char *out_dir) {
    const modtree_file_t *file = &files->files[file_index];
    const char *language = file->name ? language_for_name(file->name) : NULL;
    const Module *mod = by_file[file_index];

    char relpath[900];
    if(html_output_relpath(dirs, files, file_index, relpath, sizeof relpath) != 0) return -1;

    char full_path[1200];
    snprintf(full_path, sizeof full_path, "%s/%s", out_dir, relpath);

    char dir_only[1200];
    snprintf(dir_only, sizeof dir_only, "%s", full_path);
    char *slash = strrchr(dir_only, '/');
    if(slash) { *slash = '\0'; mkdir_p(dir_only); }

    FILE *o = fopen(full_path, "wb");
    if(!o) return -1;

    int has_diagram = 0;
    if(mod)
        for(int k = 0; k < mod->symbolCount && !has_diagram; k++)
            if(mod->symbols[k].diagram && mod->symbols[k].diagram[0]) has_diagram = 1;

    write_head(o, file->name ? file->name : "(unnamed)", has_diagram);

    if(!mod)
        fputs("<p class=\"error\">Parser failed for this file — no documentation extracted.</p>\n", o);
    else if(mod->symbolCount == 0)
        fputs("<p class=\"empty\">No documented symbols.</p>\n", o);
    if(mod)
        for(int k = 0; k < mod->symbolCount; k++) render_symbol(o, &mod->symbols[k], language);

    write_foot(o, has_diagram);

    int rc = ferror(o) ? -1 : 0;
    if(fclose(o) != 0) rc = -1;
    return rc;
}

/* Writes out_dir/index.html: page shell plus the nested directory/file tree,
 * files linking out to their own rendered pages. No diagrams live on this
 * page, so it never needs the Mermaid script. */
static int write_index(const modtree_dir_table_t *dirs, const modtree_file_table_t *files,
                        const char *out_dir, const char *title) {
    char path[1200];
    snprintf(path, sizeof path, "%s/index.html", out_dir);
    FILE *o = fopen(path, "wb");
    if(!o) return -1;

    const char *t = (title && title[0]) ? title : "Documentation";
    write_head(o, t, 0);
    fputs("<ul class=\"tree\">\n", o);

    adjacency_t a;
    adjacency_build(&a, dirs, files);
    for(int d = a.dir_root; d != -1; d = a.dir_sib[d])
        render_index_tree(o, &a, dirs, files, d);
    for(int f = a.file_root; f != -1; f = a.file_sib[f]) {
        char relpath[900];
        if(html_output_relpath(dirs, files, (size_t)f, relpath, sizeof relpath) == 0) {
            fputs("<li><a href=\"", o);
            put_escaped(o, relpath);
            fputs("\">", o);
            put_escaped(o, files->files[f].name ? files->files[f].name : "(unnamed)");
            fputs("</a></li>\n", o);
        }
    }
    adjacency_free(&a);

    fputs("</ul>\n", o);
    write_foot(o, 0);

    int rc = ferror(o) ? -1 : 0;
    if(fclose(o) != 0) rc = -1;
    return rc;
}

//Renders the tree as one out_dir/index.html plus one rendered page per file (embedded CSS; Mermaid JS is pulled in per-page only when that file carries block diagrams).
int html_render(const modtree_dir_table_t *dirs, const modtree_file_table_t *files,
                 const Module *modules, size_t module_count,
                 const char *out_dir, const char *title) {
    mkdir_p(out_dir);

    const Module **by_file = build_module_index(modules, module_count, files->count);

    int rc = 0;
    for(size_t i = 0; i < files->count; i++) {
        if(write_file_page(dirs, files, by_file, i, out_dir) != 0) { rc = -1; break; }
    }

    free(by_file);
    if(rc != 0) return rc;
    return write_index(dirs, files, out_dir, title);
}
