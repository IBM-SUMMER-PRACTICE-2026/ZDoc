#include "options.h"

#include <ctype.h>
#include <string.h>


void zd_options_init(zd_options *o) {
    memset(o, 0, sizeof *o);
    o->mode = ZD_MODE_OFFLINE;
    o->format = ZD_FORMAT_MD;
    strcpy(o->out_dir, "./zdoc-out");
    strcpy(o->bob_cli, "bob");
}

const char *zd_mode_name(zd_mode m) {
    return m == ZD_MODE_AI ? "ai" : "offline";
}

const char *zd_format_name(zd_format f) {
    return f == ZD_FORMAT_HTML ? "html" : "md";
}

/* Language table adapted from Aleksandar's CLI (feature/CLI d11084e). */
typedef struct {
    const char *name;  /* canonical */
    const char *alias; /* accepted alternative, or NULL */
} zd_lang_def;

static const zd_lang_def zd_langs[] = {
    { "plx",    NULL },
    { "plas",   NULL },
    { "c",      NULL },
    { "cpp",    "c++" },
    { "java",   NULL },
    { "asm",    "assembler" },
    { "pascal", NULL },
};

static int zd_strieq(const char *a, const char *b) {
    for(; *a && *b; a++, b++)
        if(tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
    return *a == *b;
}

const char *zd_lang_canonical(const char *name) {
    size_t i;

    for(i = 0; i < sizeof zd_langs / sizeof zd_langs[0]; i++) {
        if(zd_strieq(name, zd_langs[i].name)) return zd_langs[i].name;
        if(zd_langs[i].alias && zd_strieq(name, zd_langs[i].alias)) return zd_langs[i].name;
    }
    return NULL;
}

const char *zd_lang_supported(void) {
    return "plx, plas, c, cpp (c++), java, asm (assembler), pascal";
}