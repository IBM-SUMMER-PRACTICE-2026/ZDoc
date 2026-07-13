#include "request.h"

#include <stdio.h>
#include <sys/stat.h>

int zd_request_validate(const zd_options *o) {
    struct stat st;
    size_t i;
    int bad = 0;

    for(i = 0; i < o->n_inputs; i++) {
        if(stat(o->inputs[i], &st) != 0) {
            fprintf(stderr, "zdoc: %s: no such file or directory\n", o->inputs[i]);
            bad = 1;
        }
    }
    return bad ? -1 : 0;
}

/* JSON string literal with escaping, so paths with backslashes and
   titles with quotes survive intact. */
static void zd_json_string(FILE *out, const char *s) {
    fputc('"', out);
    for(; *s; s++) {
        unsigned char c = (unsigned char)*s;

        switch(c) {
        case '"':  fputs("\\\"", out); break;
        case '\\': fputs("\\\\", out); break;
        case '\n': fputs("\\n", out);  break;
        case '\r': fputs("\\r", out);  break;
        case '\t': fputs("\\t", out);  break;
        default:
            if(c < 0x20) fprintf(out, "\\u%04x", c);
            else fputc(c, out);
        }
    }
    fputc('"', out);
}

/* Array of n strings laid out in rows of `stride` bytes - lets one helper
   serve excludes and inputs despite their different row widths. */
static void zd_json_array(FILE *out, const char *base, size_t stride, size_t n) {
    size_t i;

    fputc('[', out);
    for(i = 0; i < n; i++) {
        if(i) fputs(", ", out);
        zd_json_string(out, base + i * stride);
    }
    fputc(']', out);
}

/* Array of n strings addressed through real pointers (languages: each
   entry is its own allocation, not a fixed-width row - see zd_options_init). */
static void zd_json_ptr_array(FILE *out, char *const *items, size_t n) {
    size_t i;

    fputc('[', out);
    for(i = 0; i < n; i++) {
        if(i) fputs(", ", out);
        zd_json_string(out, items[i]);
    }
    fputc(']', out);
}

/* Array of n strings addressed through real pointers (languages: each
   entry is its own allocation, not a fixed-width row - see zd_options_init). */
static void zd_json_ptr_array(FILE *out, char *const *items, int n) {
    int i;

    fputc('[', out);
    for(i = 0; i < n; i++) {
        if(i) fputs(", ", out);
        zd_json_string(out, items[i]);
    }
    fputc(']', out);
}

void zd_request_write(const zd_options *o, FILE *out) {
    fputs("{\n", out);
    fprintf(out, "  \"zdoc_request\": \"generate\",\n");
    fprintf(out, "  \"client_version\": \"%s\",\n", ZD_VERSION);
    fprintf(out, "  \"mode\": \"%s\",\n", zd_mode_name(o->mode));
    fprintf(out, "  \"output_format\": \"%s\",\n", zd_format_name(o->format));

    fputs("  \"out_dir\": ", out);
    zd_json_string(out, o->out_dir);
    fputs(",\n", out);

    fputs("  \"title\": ", out);
    zd_json_string(out, o->title);
    fputs(",\n", out);

    fprintf(out, "  \"recursive\": %s,\n", o->recursive ? "true" : "false");
    fprintf(out, "  \"no_source\": %s,\n", o->no_source ? "true" : "false");

    fputs("  \"languages\": ", out);
    zd_json_ptr_array(out, o->languages, o->n_languages);
    fputs(",\n", out);

    fputs("  \"exclude\": ", out);
    zd_json_ptr_array(out, o->excludes, o->n_excludes);
    fputs(",\n", out);

    fputs("  \"bob_cli\": ", out);
    zd_json_string(out, o->bob_cli);
    fputs(",\n", out);

    fputs("  \"bob_args\": ", out);
    zd_json_string(out, o->bob_args);
    fputs(",\n", out);

    fputs("  \"sources\": ", out);
    zd_json_array(out, &o->inputs[0][0], ZD_PATH_MAX, o->n_inputs);
    fputs("\n}\n", out);
}
