#include "cli.h"

#include <stdio.h>
#include <string.h>

//function to print help message
static void zd_print_help(void) {
    printf(
"zdoc %s - documentation generator for legacy and modern languages\n"
"\n"
"usage: zdoc [options] <source_dir>\n"
"\n"
"options:\n"
"  --mode offline|ai         Operating mode (default: offline)\n"
"  --output-format md|html   Output format (default: md)\n"
"  --out-dir <path>          Directory for output files (default: ./zdoc-out)\n"
"  --lang <ext>[,<ext>]      Restrict processing to files with these extensions,\n"
"                            comma-separated, dot included (e.g. --lang .c,.cpp)\n"
"  --recursive               Recurse into subdirectories\n"
"  --exclude <glob>          Exclude files matching glob (repeatable)\n"
"  --bob-cli <path>          Path to Bob CLI binary (default: bob on PATH)\n"
"  --bob-args <args>         Additional arguments forwarded to Bob CLI\n"
"  --title <string>          Project title shown in the output\n"
"  --no-source               Omit source snippets from output\n"
"  --version                 Print ZDoc version\n"
"  --help                    Print this help\n"
"\n"
"Supported languages: c/c++, java, plx, plxmac\n"
"\n"
"Configuration may also be supplied via zdoc.yaml or zdoc.json in the\n"
"working directory; command-line options override it. See docs/ZDOC.md.\n",ZD_VERSION
    );
}


//what ZDoc is, shown when zdoc is run with no arguments at all
static void zd_print_about(void) {
    printf(
"ZDoc %s - documentation generator for legacy and modern languages\n"
"\n"
"ZDoc extracts structured documentation from source written in PL/X,\n"
"PLAS, C, C++, Java, Assembler and Pascal - similar to Doxygen or\n"
"JavaDoc, but purpose-built for mainframe and mixed-language codebases.\n"
"Each source module becomes an expandable node in the output, with\n"
"per-symbol signatures, briefs, parameter tables and notes.\n"
"\n"
"Modes:   offline (default)      parse sources only, no external calls\n"
"         ai                     adds Bob CLI block diagrams per function\n"
"Output:  Markdown or a single self-contained HTML page\n"
"\n"
"usage:   zdoc [options] <source_dir>\n"
"example: zdoc --output-format html --title \"My Project\" ./src\n"
"\n"
"Run 'zdoc --help' for all options. Full spec: docs/ZDOC.md\n",
        ZD_VERSION);
}

//Value of an option given as "--opt=value" or "--opt value"

static const char *zd_take_value(int argc, char **argv, int *i, const char *name, const char *inline_value) {
    
    if(inline_value) return inline_value;

    if(*i + 1 < argc) return argv[++*i];

    fprintf(stderr, "zdoc: option '%s' requires a value\n", name);
    return NULL;
}


zd_cli_result zd_cli_parse(int argc, char **argv, zd_options *o) {
    //Lists given on the command line replace (not extend) config lists, so we clear them here.
    int cli_langs_seen = 0, cli_excludes_seen = 0;
    int i;

    if(argc == 1) { //bare "zdoc" - introduce the tool
        zd_print_about();
        return ZD_CLI_EXIT;
    }

    for(i = 1; i< argc; i++) {
        const char *arg = argv[i];
        char name[64];
        const char *inline_value = NULL;
        const char *eq, *v;

        if(arg[0] != '-' || strcmp(arg, "-") == 0) {
            if(o->n_inputs >= ZD_MAX_INPUTS) {
                fprintf(stderr, "zdoc: too many source paths (max %d)\n", ZD_MAX_INPUTS);
                return ZD_CLI_ERROR;
            }
            snprintf(o->inputs[o->n_inputs++], ZD_PATH_MAX, "%s", arg);
            continue;
        }

        eq = strchr(arg, '=');
        if(eq) {
            size_t n = (size_t)(eq - arg);
            if(n >= sizeof name) n = sizeof name - 1;
            memcpy(name, arg, n);
            name[n] = '\0';
            inline_value = eq + 1;
        } 
        else {
            snprintf(name, sizeof name, "%s", arg);
        }

        if(strcmp(name, "--help") == 0 || strcmp(name, "-h") == 0) {
            zd_print_help();
            return ZD_CLI_EXIT;
        }

        if(strcmp(name , "--version") == 0) {
            printf("zdoc %s\n", ZD_VERSION);
            return ZD_CLI_EXIT;
        }

        if(strcmp(name, "--recursive") == 0) {
            o->recursive = 1;
            continue;
        }
        
        if(strcmp(name, "--no-source") == 0) {
            o->no_source = 1;
            continue;
        }

        //Every remaining option takes a value.

        v = zd_take_value(argc, argv, &i, name, inline_value);

        if(!v) return ZD_CLI_ERROR;

        if(strcmp(name, "--mode") == 0) {
            if(strcmp(v, "offline") == 0) o->mode = ZD_MODE_OFFLINE;
            else if(strcmp(v, "ai") == 0) o->mode = ZD_MODE_AI;
            else {
                fprintf(stderr, "zdoc: --mode must be 'offline' or 'ai' (got '%s')\n", v);
                return ZD_CLI_ERROR;
            }
        } else if(strcmp(name, "--output-format") == 0) {
            if(strcmp(v, "md") == 0) o->format = ZD_FORMAT_MD;
            else if(strcmp(v, "html") == 0) o->format = ZD_FORMAT_HTML;
            else {
                fprintf(stderr, "zdoc: --output-format must be 'md' or 'html' (got '%s')\n", v);
                return ZD_CLI_ERROR;
            }

        } else if(strcmp(name, "--out-dir") == 0) {
            snprintf(o->out_dir, ZD_PATH_MAX, "%s", v);
        }else if(strcmp(name, "--title") == 0) {
            snprintf(o->title, ZD_TITLE_MAX, "%s" ,v);
        }else if(strcmp(name, "--bob-cli") == 0) {
            snprintf(o->bob_cli, ZD_PATH_MAX, "%s", v);
        }else if(strcmp(name, "--bob-args") == 0) {
            snprintf(o->bob_args, sizeof o->bob_args, "%s", v);
        }else if(strcmp(name, "--exclude") == 0) { 
            if(!cli_excludes_seen) {
                o->n_excludes = 0;
                cli_excludes_seen =1;
            }
            if(o->n_excludes < ZD_MAX_EXCLUDES) snprintf(o->excludes[o->n_excludes++],ZD_GLOB_MAX, "%s", v);
                
        }else if(strcmp(name, "--lang") == 0) {
            char tmp[ZD_ARGS_MAX];
            char *tok;

            if(!cli_langs_seen) {
                o->n_languages = 0;
                cli_langs_seen =1;
            }

            snprintf(tmp, sizeof tmp, "%s", v);
            for(tok = strtok(tmp, ", \t"); tok; tok = strtok(NULL, ", \t")) {
                if(o->n_languages < ZD_MAX_LANGS)
                    snprintf(o->languages[o->n_languages++], ZD_LANG_MAX, "%s", tok);
            }

        }else {
            fprintf(stderr, "zdoc: unknown option '%s' (see zdoc --help)\n", name);
            return ZD_CLI_ERROR;
        }
    }

    if(o->n_inputs == 0) {
        fprintf(stderr, "zdoc: no source directory given\n"
        "usage: zdoc [options] <source_dir>\n");
        return ZD_CLI_ERROR;
    } 

    return ZD_CLI_OK;
}