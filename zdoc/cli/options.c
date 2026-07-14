#include "options.h"

#include <stdlib.h>
#include <string.h>


/**
 * @brief Initialize o with built-in default option values.
 *
 * Zeroes o, then sets mode to ZD_MODE_OFFLINE, format to ZD_FORMAT_MD,
 * out_dir to "./zdoc-out", bob_cli to "bob", and allocates the
 * languages/excludes string arrays (ZD_MAX_LANGS and ZD_MAX_EXCLUDES
 * entries respectively) that zd_config_load() and zd_cli_parse() fill in
 * later.
 *
 * @param o Options struct to initialize; any prior contents are
 *          discarded.
 * @return ZDOC_OK on success, or ZDOC_OUT_OF_MEMORY if any allocation
 *         failed.
 */
enum ZDoc_Error zd_options_init(zd_options *o) {
    int i;

    memset(o, 0, sizeof *o);
    o->mode = ZD_MODE_OFFLINE;
    o->format = ZD_FORMAT_MD;
    strcpy(o->out_dir, "./zdoc-out");
    strcpy(o->bob_cli, "bob");

    o->languages = malloc(ZD_MAX_LANGS * sizeof *o->languages);
    if (!o->languages) return ZDOC_OUT_OF_MEMORY;
    for(i = 0; i < ZD_MAX_LANGS; i++) {
        o->languages[i] = malloc(ZD_LANG_MAX);
        if (!o->languages[i]) return ZDOC_OUT_OF_MEMORY;
    }

    o->excludes = malloc(ZD_MAX_EXCLUDES * sizeof *o->excludes);
    if (!o->excludes) return ZDOC_OUT_OF_MEMORY;
    for(i = 0; i < ZD_MAX_EXCLUDES; i++) {
        o->excludes[i] = malloc(ZD_GLOB_MAX);
        if (!o->excludes[i]) return ZDOC_OUT_OF_MEMORY;
    }

    return ZDOC_OK;
}

/**
 * @brief Get the config/JSON name for a zd_mode value.
 *
 * @param m Mode value to name.
 * @return "ai" for ZD_MODE_AI, "offline" for anything else.
 */
const char *zd_mode_name(zd_mode m) {
    return m == ZD_MODE_AI ? "ai" : "offline";
}

/**
 * @brief Get the config/JSON name for a zd_format value.
 *
 * @param f Format value to name.
 * @return "html" for ZD_FORMAT_HTML, "md" for anything else.
 */
const char *zd_format_name(zd_format f) {
    return f == ZD_FORMAT_HTML ? "html" : "md";
}