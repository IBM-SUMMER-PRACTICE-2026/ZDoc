#include "config.h"

#include <stdio.h>
#include <string.h>

#define ZD_CFG_LINE_MAX 1024
#define ZD_CFG_JSON_MAX 16384

static char *zd_trim(char *s) {
    char *end;
    while(*s == ' ' || *s == '\t') s++;
    end = s + strlen(s);

    while(end > s && (end[-1] == ' '|| end [-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) *--end = '\0';
    return s;
}

static void zd_strip_comment(char *s) {
    int in_single = 0, in_double = 0;
    char *p;

    for(p = s; *p; p++) {
        if(*p == '\'' && !in_double) in_single = !in_single;
        else if(*p == '"' && !in_single) in_double = !in_double;
        else if(*p == '#' && !in_single && !in_double) {
            *p = '\0';
            return;
        }
    }
}

static char *zd_unquote(char *s) {
    size_t n = strlen(s);

    if(n >= 2 && ((s[0] == '"' && s[n-1] =='"') || 
    (s[0] == '\'' && s[n-1] == '\''))) {
        s[n-1] = '\0';
        s++;
    }
    return s;
}

static int zd_bool(const char *v) {
    return strcmp(v, "true") == 0 || strcmp(v, "yes") == 0 || strcmp(v, "on") == 0;
}

static void zd_set_scalar(zd_options *o, const char *key, char *val, const char *file , int line) {
    val = zd_unquote(val);

     if (strcmp(key, "title") == 0) snprintf(o->title, sizeof o->title, "%s", val);
    else if (strcmp(key, "out_dir") == 0) snprintf(o->out_dir, sizeof o->out_dir, "%s",val);
    else if (strcmp(key, "bob_cli") == 0) snprintf(o->bob_cli, sizeof o->bob_cli, "%s",val);
    else if (strcmp(key, "bob_args") == 0) snprintf(o->bob_args, sizeof o->bob_args, "%s", val);
    else if (strcmp(key, "recursive") == 0) o->recursive = zd_bool(val);
    else if (strcmp(key, "no_source") == 0) o->no_source = zd_bool(val);
    else if (strcmp(key, "mode") == 0) {
        if (strcmp(val, "offline") == 0)
            o->mode = ZD_MODE_OFFLINE;
        else if (strcmp(val, "ai") == 0)
            o->mode = ZD_MODE_AI;
        else
            fprintf(stderr, "zdoc: %s:%d: bad mode '%s'\n", file, line, val);
    } else if (strcmp(key, "output_format") == 0) {
        if (strcmp(val, "md") == 0)
            o->format = ZD_FORMAT_MD;
        else if (strcmp(val, "html") == 0)
            o->format = ZD_FORMAT_HTML;
        else
            fprintf(stderr, "zdoc: %s:%d: bad output_format '%s'\n", file, line, val);
    } else {
        fprintf(stderr, "zdoc: %s:%d: unknown key '%s' ignored\n", file, line, key);
    }
}

typedef struct {
    const char *p;
    int line;
} zd_json;

static void zd_json_ws(zd_json *j) {
    while(*j->p == ' ' || *j->p == '\t' || *j->p  == '\r' || *j->p == '\n') {
        if(*j->p == '\n') j->line++;
        j->p++;
    }
}

static int zd_json_err(zd_json *j, const char *msg) {
    fprintf(stderr, "zdoc: zdoc.json:%d: %s\n", j->line, msg);
    return 0;
}

//Parse a string at the cursor into the buffer handling \" \\ \/ \n \t \r
static int zd_json_str(zd_json *j, char *buf, size_t cap) {
    size_t n = 0;
    
    if(*j->p != '"') return zd_json_err(j, "expected string");
    j->p++;

    while(*j->p && *j->p != '"') {
        char c = *j->p++;

        if(c == '\\') {
            char e = *j->p;

            if(e == '\0') break;
            j->p++;

            if(e == 'n') c = '\n';
            else if(e == 't') c = '\t';
            else if(e == 'r') c = '\r'; 
            else c = e; // covers \" \\ \/ but unknown ones pass through
        }
        if(n + 1 < cap) buf[n++] = c; 
    }
    buf[n] = '\0';
    
    if(*j->p != '"') return zd_json_err(j, "unterminated string");
    j->p++;
    return 1;
}

static int zd_json_value(zd_json *j, zd_options *o, const char *key) {
    zd_json_ws(j);

    if(*j->p == '"') {



        char val[ZD_ARGS_MAX];

        if(!zd_json_str(j, val, sizeof val)) return 0;
        zd_set_scalar(o, key, val, "zdoc.json", j->line);
        return 1;
    }

    if(strncmp(j->p, "true", 4) == 0 || strncmp(j->p, "false", 5) == 0) {
        int b = (*j->p == 't');
        
        j->p += b ? 4 : 5;

        if(strcmp(key, "recursive") == 0) o->recursive = b;
        else if(strcmp(key, "no_source") == 0) o->no_source = b;
        else fprintf(stderr, "zdoc: zdoc.json:%d: key '%s' does not take a boolean, ignored\n", j->line, key);
        return 1;
    }
    //string array for languages(exclude)
    if(*j->p == '[') {
        int is_lang = strcmp(key, "languages") == 0;
        int is_excl = strcmp(key, "exclude") == 0;
        char item[ZD_GLOB_MAX];

        if(is_lang) o->n_languages = 0; //file lists replace defaults
        else if(is_excl) o->n_excludes = 0;
        else fprintf(stderr, "zdoc: zdoc.json:%d: key '%s' does not take a list, ignored\n",j->line, key);

        j->p++;
        zd_json_ws(j);

        if(*j->p == ']') {
            j->p++;
            return 1;
        } 

        for(;;) {
            if(!zd_json_str(j, item, sizeof item)) return 0;

            if(is_lang && *item) {
                const char *canon = zd_lang_canonical(item);

                if(!canon)
                    fprintf(stderr, "zdoc: zdoc.json:%d: unknown language '%s' ignored (supported: %s)\n", j->line, item, zd_lang_supported());
                else if(o->n_languages < ZD_MAX_LANGS)
                    snprintf(o->languages[o->n_languages++], ZD_LANG_MAX, "%s", canon);
            }
            else if(is_excl && *item && o->n_excludes < ZD_MAX_EXCLUDES) snprintf(o->excludes[o->n_excludes++],ZD_GLOB_MAX,"%s", item);

            zd_json_ws(j);
            
            if(*j->p == ',') {
                j->p++; 
                zd_json_ws(j); 
                continue; 
            }
            if(*j->p == ']') {
                j->p++;
                return 1;
            }

            return zd_json_err(j,"expected ',' or ']' in list");
        }
    }

    return zd_json_err(j, "unsupported value (use strings, true/false or string lists)");
}

static void zd_json_load(FILE *f, zd_options *o) {
    char buf[ZD_CFG_JSON_MAX];
    char key[64]; 
    size_t n = fread(buf,1, sizeof buf - 1, f);
    zd_json j;

    buf[n] = '\0';

    if(n == sizeof buf - 1)
        fprintf(stderr, "zdoc: warning: zdoc.json larger than %d bytes, rest ignored\n", (int)(sizeof buf - 1));

    j.p = buf;
    j.line = 1;

    zd_json_ws(&j);

    if(*j.p != '{') {
        zd_json_err(&j, "expected '{'");
        return;
    }
    j.p++;
    zd_json_ws(&j);
    if(*j.p == '}') return;

    for(;;) {
        if(!zd_json_str(&j, key, sizeof key)) return;
        zd_json_ws(&j);
        if(*j.p != ':') {
            zd_json_err(&j,"expected ':' after key");
            return;
        }
        j.p++;
        if(!zd_json_value(&j, o, key)) return;

        zd_json_ws(&j);
        if(*j.p == ',') {
            j.p++;
            zd_json_ws(&j);
            continue;
        }

        if(*j.p == '}') {
            return;
        }

        zd_json_err(&j, "expected ',' or '}' after value");
        return;
    }
}


void zd_config_load(zd_options *o) {
    FILE *f;
    char line[ZD_CFG_LINE_MAX];
    char list_key[16] = "";
    int lineno = 0;

    f = fopen("zdoc.yaml","r");
    if(!f) {
        f = fopen("zdoc.json", "r");
        if(f) {
            zd_json_load(f, o);
            fclose(f);
        }
        return;
    }
    
    while(fgets(line, sizeof line, f)) {
        char *s, *colon, *key, *val;

        lineno ++;

        zd_strip_comment(line);
        s = zd_trim(line);

        if(!*s) continue;

        /* "- item" list entry under languages: / exclude: */

        if(s[0] == '-' &&(s[1] == ' ' || s[1] == '\t' || s[1] == '\0')) {
            char *item = zd_unquote(zd_trim(s + 1));

             if (strcmp(list_key, "languages") == 0) {
                const char *canon = *item ? zd_lang_canonical(item) : NULL;

                if (*item && !canon)
                    fprintf(stderr, "zdoc: zdoc.yaml:%d: unknown language '%s' ignored (supported: %s)\n", lineno, item, zd_lang_supported());
                else if (canon && o->n_languages < ZD_MAX_LANGS)
                    snprintf(o->languages[o->n_languages++], ZD_LANG_MAX, "%s", canon);
            } else if (strcmp(list_key, "exclude") == 0) {
                if (*item && o->n_excludes < ZD_MAX_EXCLUDES) snprintf(o->excludes[o->n_excludes++], ZD_GLOB_MAX, "%s", item);
            } else {
                fprintf(stderr, "zdoc: zdoc.yaml:%d: stray list item ignored\n", lineno);
            }
            continue;
        }
        colon = strchr(s, ':');
        if(!colon) {
            fprintf(stderr ,"zdoc: zdoc.yaml:%d: not a 'key: value' line, ignored\n", lineno);
            continue;
        }
        *colon = '\0';
        key = zd_trim(s);
        val = zd_trim(colon + 1);

        if(!*val) {
            /* "languages: " / "exclude; " open a list; config lists replace the defaults and the CLI later replaces these in turn*/
            if(strcmp(key, "languages") == 0) {
                o->n_languages = 0;
                strcpy(list_key, "languages");
            } else if(strcmp(key, "exclude") == 0) {
                o->n_excludes = 0;
                strcpy(list_key, "exclude");
            } else {
                list_key[0] = '\0';
                fprintf(stderr, "zdoc: zdoc.yaml:%d: key '%s' has no value, ignored\n", lineno, key);
            }
            continue;
        }
        list_key[0] = '\0';
        zd_set_scalar(o, key, val,"zdoc.yaml", lineno);
    }
    fclose(f);
}