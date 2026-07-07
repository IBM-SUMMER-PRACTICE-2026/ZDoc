/* Emits a DxModel as the combined documentation-model JSON. No parsing
 * here in either direction - see child_parser.c (reads a child parser's
 * JSON) and doc_extractor.c (builds the DxModel this writes). */
#include "doc_extractor.h"

static void wstr(FILE *o, const char *s) {
    fputc('"', o);
    for(const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) {
        switch(*p) {
            case '"':  fputs("\\\"", o); break;
            case '\\': fputs("\\\\", o); break;
            case '\n': fputs("\\n", o); break;
            case '\r': fputs("\\r", o); break;
            case '\t': fputs("\\t", o); break;
            default:
                if(*p < 0x20) fprintf(o, "\\u%04x", *p);
                else fputc(*p, o);
        }
    }
    fputc('"', o);
}

static void write_symbol(FILE *o, const DxSymbol *s) {
    fputs("{\"kind\":", o);
    wstr(o, s->kind);
    fprintf(o, ",\"line\":%u,\"name\":", s->line);
    wstr(o, s->name);
    fputs(",\"signature\":", o);
    wstr(o, s->signature);

    fputs(",\"doc\":{", o);
    int first = 1;
    if(s->brief) { fputs("\"brief\":", o); wstr(o, s->brief); first = 0; }
    if(s->param_count) {
        if(!first) fputc(',', o);
        fputs("\"params\":[", o);
        for(size_t i = 0; i < s->param_count; i++) {
            if(i) fputc(',', o);
            fputs("{\"name\":", o);
            wstr(o, s->params[i].name);
            fputs(",\"desc\":", o);
            wstr(o, s->params[i].desc);
            fputc('}', o);
        }
        fputc(']', o);
        first = 0;
    }
    if(s->returns) { if(!first) fputc(',', o); fputs("\"returns\":", o); wstr(o, s->returns); first = 0; }
    if(s->notes) { if(!first) fputc(',', o); fputs("\"notes\":", o); wstr(o, s->notes); }
    fputs("}}", o);
}

void dx_write(const DxModel *m, FILE *o) {
    fputs("{\"dirs\":[", o);
    for(size_t i = 0; i < m->dir_count; i++) {
        if(i) fputc(',', o);
        fputs("{\"name\":", o);
        wstr(o, m->dirs[i].name);
        fprintf(o, ",\"parent_index\":%d}", m->dirs[i].parent_index);
    }
    fputs("],\"files\":[", o);
    for(size_t i = 0; i < m->file_count; i++) {
        const DxFile *f = &m->files[i];
        if(i) fputc(',', o);
        fputs("{\"name\":", o);
        wstr(o, f->name);
        fprintf(o, ",\"parent_dir_index\":%d", f->parent_dir_index);
        fputs(",\"language\":", o);
        wstr(o, f->language);
        if(f->error) fputs(",\"error\":true", o);
        fputs(",\"symbols\":[", o);
        for(size_t k = 0; k < f->symbol_count; k++) {
            if(k) fputc(',', o);
            write_symbol(o, &f->symbols[k]);
        }
        fputs("]}", o);
    }
    fputs("]}\n", o);
}
