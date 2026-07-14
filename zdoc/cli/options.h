/* options.h - shared option model for the zdoc CLI.
   One struct carries every setting from all three sources, applied in
  precedence order: built-in defaults -> zdoc.yaml -> command line
  (see docs/ZDOC.md -> "Configuration File").
 */
#ifndef ZD_OPTIONS_H
#define ZD_OPTIONS_H

#include <stddef.h>
#include <stdint.h>
#include "../error_interface.h"

#define ZD_VERSION "0.1.0"

#define ZD_PATH_MAX     1024
#define ZD_TITLE_MAX     256
#define ZD_ARGS_MAX      512
#define ZD_LANG_MAX       16
#define ZD_MAX_LANGS      16
#define ZD_GLOB_MAX      256
#define ZD_MAX_EXCLUDES   32
#define ZD_MAX_INPUTS     32

typedef enum {
    ZD_MODE_OFFLINE,
    ZD_MODE_AI
} zd_mode;

typedef enum {
    ZD_FORMAT_MD,
    ZD_FORMAT_HTML
} zd_format;

typedef struct {
    zd_mode   mode;
    zd_format format;
    char      out_dir[ZD_PATH_MAX];
    char      title[ZD_TITLE_MAX];
    char      bob_cli[ZD_PATH_MAX];
    char      bob_args[ZD_ARGS_MAX];
    char      **languages;
    size_t    n_languages;
    char      **excludes;
    size_t    n_excludes;
    uint8_t   recursive;
    uint8_t   no_source;
    char      inputs[ZD_MAX_INPUTS][ZD_PATH_MAX];
    size_t    n_inputs;
} zd_options;

/* Returns ZDOC_OK on success, ZDOC_OUT_OF_MEMORY if an allocation failed. */
enum ZDoc_Error zd_options_init(zd_options *o);

const char *zd_mode_name(zd_mode m);
const char *zd_format_name(zd_format f);

#endif
