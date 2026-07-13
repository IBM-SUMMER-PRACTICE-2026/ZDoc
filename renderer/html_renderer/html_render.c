/*
  Walks the module_tree tables and a parsed Module array directly and
  writes the result out as a single self-contained index.html: nested
  <details>/<summary> nodes for directories and files, with per-symbol
  documentation sections (signature, brief, parameters, returns, notes,
  block diagram). No JSON, no parsing, no intermediate model - matching a
  file to its parsed module and deriving its language happens right here
  instead of in a separate doc_extractor stage.
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

/* Error-checked allocation - exits the process on OOM. Kept local (rather
 * than reused from doc_extractor's xalloc.h) since this renderer no longer
 * depends on anything under extractor/doc_extractor/src/. */
static void *xmalloc(size_t n) {
    void *p = malloc(n ? n : 1);
    if(!p) {
        fprintf(stderr, "zdoc-html-renderer: out of memory\n");
        exit(1);
    }
    return p;
}

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

/* Matches file index i back to the module that parsed it, via pathIndex -
 * the file table index the daemon stamped onto the module while parsing,
 * not a string comparison. Returns NULL if no module matched (parsing
 * failed, was skipped, or hasn't run yet). */
static const Module *module_for_file(const Module *modules, size_t module_count, size_t i) {
    if(i >= module_count) return NULL;
    const Module *mod = &modules[i];
    if(!mod->filename || mod->pathIndex != (int)i) return NULL;
    return mod;
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
//Renders a file and its symbols. The caller is responsible for wrapping the top-level call in <ul>...</ul> and for the adjacency table.
static void render_file(FILE *o, const modtree_file_table_t *files,
                         const Module *modules, size_t module_count, size_t f) {
    const modtree_file_t *file = &files->files[f];
    const char *language = file->name ? language_for_name(file->name) : NULL;
    const Module *mod = module_for_file(modules, module_count, f);

    fputs("<li><details class=\"file\"><summary>", o);
    put_escaped(o, file->name ? file->name : "(unnamed)");
    fputs("</summary>\n", o);

    if(!mod)
        fputs("<p class=\"error\">Parser failed for this file — no documentation extracted.</p>\n", o);
    else if(mod->symbolCount == 0)
        fputs("<p class=\"empty\">No documented symbols.</p>\n", o);
    if(mod)
        for(int k = 0; k < mod->symbolCount; k++) render_symbol(o, &mod->symbols[k], language);
    fputs("</details></li>\n", o);
}
/*Renders a directory and its children recursively. The caller is responsible for
 wrapping the top-level call in <ul>...</ul> and for the adjacency table.*/
static void render_dir(FILE *o, const adjacency_t *a, const modtree_dir_table_t *dirs,
                        const modtree_file_table_t *files, const Module *modules,
                        size_t module_count, int d) {
    fputs("<li><details class=\"dir\" open><summary>", o);
    put_escaped(o, dirs->dirs[d].name ? dirs->dirs[d].name : "(unnamed)");
    fputs("/</summary><ul>\n", o);

    for(int c = a->dir_child[d]; c != -1; c = a->dir_sib[c])
        render_dir(o, a, dirs, files, modules, module_count, c);
    for(int f = a->file_child[d]; f != -1; f = a->file_sib[f])
        render_file(o, files, modules, module_count, (size_t)f);

    fputs("</ul></details></li>\n", o);
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

//Renders the whole tree as one self-contained out_dir/index.html (embedded CSS; Mermaid JS is pulled in only when the model carries block diagrams).
int html_render(const modtree_dir_table_t *dirs, const modtree_file_table_t *files,
                 const Module *modules, size_t module_count,
                 const char *out_dir, const char *title) {
    mkdir_p(out_dir);

    char path[1200];
    snprintf(path, sizeof path, "%s/index.html", out_dir);
    FILE *o = fopen(path, "wb");
    if(!o) return -1;

    adjacency_t a;
    adjacency_build(&a, dirs, files);

    int has_diagram = 0;
    for(size_t i = 0; i < files->count && !has_diagram; i++) {
        const Module *mod = module_for_file(modules, module_count, i);
        if(!mod) continue;
        for(int k = 0; k < mod->symbolCount; k++)
            if(mod->symbols[k].diagram && mod->symbols[k].diagram[0]) { has_diagram = 1; break; }
    }

    const char *t = (title && title[0]) ? title : "Documentation";
    fputs("<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"utf-8\">\n<title>", o);
    put_escaped(o, t);
    fputs("</title>\n<style>\n", o);
    fputs(CSS, o);
    fputs("</style>\n", o);
    if(has_diagram)
        fputs("<noscript><style>pre.mermaid{display:none}</style></noscript>\n", o);
    fputs("</head>\n<body>\n<h1>", o);
    put_escaped(o, t);
    fputs("</h1>\n<ul class=\"tree\">\n", o);

    for(int d = a.dir_root; d != -1; d = a.dir_sib[d])
        render_dir(o, &a, dirs, files, modules, module_count, d);
    for(int f = a.file_root; f != -1; f = a.file_sib[f])
        render_file(o, files, modules, module_count, (size_t)f);

    fputs("</ul>\n", o);
    if(has_diagram) {
        fputs("<script type=\"module\">\n", o);
        fputs(MERMAID_JS, o);
        fputs("</script>\n", o);
    }
    fputs("</body>\n</html>\n", o);

    adjacency_free(&a);
    int rc = ferror(o) ? -1 : 0;
    if(fclose(o) != 0) rc = -1;
    return rc;
}
