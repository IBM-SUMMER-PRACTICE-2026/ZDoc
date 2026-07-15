#include "request.h"

#include <stdio.h>
#include <sys/stat.h>

/**
 * @brief Check that every source path in o exists on disk.
 *
 * Calls stat() on each of o->inputs[0..n_inputs), reporting each missing
 * path to stderr. Checks all inputs rather than stopping at the first
 * failure, so a single run reports every bad path at once.
 *
 * @param o Options holding the resolved source paths to validate.
 * @return ZDOC_OK if every path exists, or ZDOC_FS_WALK_FAILED if at
 *         least one did not.
 */
enum ZDoc_Error zd_request_validate(const zd_options *o) {
    struct stat st;
    size_t i;
    enum ZDoc_Error status = ZDOC_OK;

    for(i = 0; i < o->n_inputs; i++) {
        if(stat(o->inputs[i], &st) != 0) {
            fprintf(stderr, "zdoc: %s: no such file or directory\n", o->inputs[i]);
            status = ZDOC_FS_WALK_FAILED;
        }
    }
    return status;
}

/**
 * @brief Write s to out as an escaped JSON string literal.
 *
 * Escapes '"', '\\', '\n', '\r', '\t', and any other control character
 * (as \\uXXXX), so paths with backslashes and titles with quotes survive
 * intact.
 *
 * @param out Output stream to write to.
 * @param s String to encode.
 */
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

/**
 * @brief Write a JSON array of n fixed-width string rows.
 *
 * Lets one helper serve excludes and inputs despite their different row
 * widths: each element is read from base + i * stride rather than
 * through a pointer array.
 *
 * @param out Output stream to write to.
 * @param base Address of the first row.
 * @param stride Byte width of each row.
 * @param n Number of rows to write.
 */
static void zd_json_array(FILE *out, const char *base, size_t stride, size_t n) {
    size_t i;

    fputc('[', out);
    for(i = 0; i < n; i++) {
        if(i) fputs(", ", out);
        zd_json_string(out, base + i * stride);
    }
    fputc(']', out);
}

/**
 * @brief Write a JSON array of n independently-allocated strings.
 *
 * Unlike zd_json_array(), each element is reached through its own
 * pointer rather than a fixed-width row - the shape languages and
 * excludes use (see zd_options_init()).
 *
 * @param out Output stream to write to.
 * @param items Array of n string pointers.
 * @param n Number of entries in items.
 */
static void zd_json_ptr_array(FILE *out, char *const *items, size_t n) {
    size_t i;

    fputc('[', out);
    for(i = 0; i < n; i++) {
        if(i) fputs(", ", out);
        zd_json_string(out, items[i]);
    }
    fputc(']', out);
}

/**
 * @brief Write o as the daemon's "generate" request JSON to out.
 *
 * Placeholder wire format for the not-yet-implemented daemon transport;
 * currently invoked (commented out) from main() to print the resolved
 * request to stdout. Emits every field of o, JSON-escaped via
 * zd_json_string/zd_json_array/zd_json_ptr_array as appropriate.
 *
 * @param o Options to serialize.
 * @param out Output stream to write the JSON object to.
 */
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
