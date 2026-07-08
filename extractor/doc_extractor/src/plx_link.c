#include "plx_link.h"
#include "json_read.h" /* xmalloc */

/* helpers.h must precede plx_parser.h - DocBlock (inside plx_parser.h)
 * uses StrBuf/StrList without including helpers.h itself. */
#include "../../../parser/plx_parser/helpers.h"
#include "../../../parser/plx_parser/plx_parser.h"

#include <string.h>

/* strdup isn't ISO C - see doc_extractor.c's identical helper. Named
 * differently from xstrdup since helpers.h (pulled in above for DocBlock's
 * StrBuf/StrList types) also declares an xstrdup - this file doesn't use
 * PL/X's own allocation helpers, only its Module/Symbol types, so there's no
 * reason to fight that name. */
static char *plxlink_strdup(const char *s) {
    size_t n = strlen(s);
    char *p = xmalloc(n + 1);
    memcpy(p, s, n + 1);
    return p;
}

/* PL/X's Symbol has no analog to DxSymbol's kind/line: every PL/X routine
 * is reported as "procedure" (one of the kinds parser/README.md documents),
 * and line is 0 - the PL/X parser only uses a line number for its own
 * diagnostic messages while parsing, it's never stored on Symbol. "output"
 * is PL/X's closest concept to a return-value description, so it maps to
 * "returns"; "description" maps to "brief". There's no PL/X equivalent of
 * doc_extractor's "notes". */
static void convert_symbol(const Symbol *src, DxSymbol *dst) {
    memset(dst, 0, sizeof *dst);
    dst->kind = plxlink_strdup("procedure");
    dst->line = 0;
    dst->name = src->name ? plxlink_strdup(src->name) : NULL;
    dst->signature = src->signature ? plxlink_strdup(src->signature) : NULL;
    dst->brief = src->description ? plxlink_strdup(src->description) : NULL;
    dst->returns = src->output ? plxlink_strdup(src->output) : NULL;

    if(src->inputCount > 0) {
        dst->params = xmalloc((size_t)src->inputCount * sizeof(DxParam));
        dst->param_count = (size_t)src->inputCount;
        for(int i = 0; i < src->inputCount; i++) {
            dst->params[i].name = src->input[i].name ? plxlink_strdup(src->input[i].name) : NULL;
            dst->params[i].desc = src->input[i].description ? plxlink_strdup(src->input[i].description) : NULL;
        }
    }
}

int run_plx_linked_batch(const char *const *paths, DxFile **targets, size_t count) {
    for(size_t i = 0; i < count; i++) {
        Module *mod = plx_parse_file(paths[i]);
        if(!mod) continue; /* leave targets[i] untouched - stays error=1 */

        DxSymbol *symbols = mod->symbolCount
            ? xmalloc((size_t)mod->symbolCount * sizeof(DxSymbol)) : NULL;
        for(int k = 0; k < mod->symbolCount; k++)
            convert_symbol(&mod->symbols[k], &symbols[k]);

        targets[i]->symbols = symbols;
        targets[i]->symbol_count = (size_t)mod->symbolCount;
        targets[i]->error = 0;

        plx_free_module(mod);
    }
    return 1;
}
