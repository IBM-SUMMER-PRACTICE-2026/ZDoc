#include "parser_shared.h"
#include <stdio.h>
#include <string.h>

/* Render a possibly-NULL string as "(null)" so every field prints. */
static const char *or_null(const char *s)
{
    return s ? s : "(null)";
}

void *xmalloc(size_t n)
{
    void *p = malloc(n);
    if (!p) {
        fprintf(stderr, "zdoc: out of memory\n");
        exit(1);
    }
    return p;
}

void *xrealloc(void *p, size_t n)
{
    p = realloc(p, n);
    if (!p) {
        fprintf(stderr, "zdoc: out of memory\n");
        exit(1);
    }
    return p;
}

char *xstrdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = xmalloc(n);
    memcpy(p, s, n);
    return p;
}



/*********************************************/
/*             INPUT PARAMS                  */
/*********************************************/
void symbol_add_input(Symbol *sym, const char *name, const char *description) {
    sym->input = xrealloc(sym->input,
                           (size_t)(sym->inputCount + 1) * sizeof(InputParam));
    sym->input[sym->inputCount].name = xstrdup(name);
    sym->input[sym->inputCount].description = xstrdup(description);
    sym->inputCount++;
}



/*********************************************/
/*                 SYMBOL                    */
/*********************************************/
void free_symbol_content(Symbol * sym) {
    free(sym->name);
    free(sym->description);
    free(sym->signature);
    free(sym->output);
    free(sym->notes);
    free(sym->type);
    free(sym->diagram);
    for (int i = 0; i < sym->inputCount; i++) {
        free(sym->input[i].name);
        free(sym->input[i].description);
    }
    free(sym->input);
}



/*********************************************/
/*                MODULE                     */
/*********************************************/
Module * init_module(const char *path) {
    Module * mod = xmalloc(sizeof(Module));
    mod->filename = xstrdup(path);
    mod->symbols = NULL;
    mod->symbolCount = 0;
    mod->symbolCap = 0;
    return mod;
}


void free_module(Module *mod)
{
    int i, j;

    if (!mod)
        return;
    for (i = 0; i < mod->symbolCount; i++) {
        free_symbol_content(&mod->symbols[i]);
    }
    free(mod->symbols);
    free(mod->filename);
    free(mod);
}


Symbol *module_add_symbol(Module *mod)
{
    Symbol *sym;

    if (mod->symbolCount == mod->symbolCap) {
        mod->symbolCap = mod->symbolCap ? mod->symbolCap * 2 : 8;
        mod->symbols = xrealloc(mod->symbols,
                                (size_t)mod->symbolCap * sizeof(Symbol));
    }
    sym = &mod->symbols[mod->symbolCount++];
    memset(sym, 0, sizeof(*sym));
    return sym;
}


void module_shrink_to_fit(Module *mod)
{
    if (mod->symbolCount == mod->symbolCap)
        return;
    if (mod->symbolCount == 0) {
        free(mod->symbols);
        mod->symbols = NULL;
    } else {
        mod->symbols = xrealloc(mod->symbols,
                                (size_t)mod->symbolCount * sizeof(Symbol));
    }
    mod->symbolCap = mod->symbolCount;
}



/*********************************************/
/*            OUTPUT / CLEANUP               */
/*********************************************/
void print_module(const Module *mod)
{
    int i, j;

    printf("Module: %s\n", mod->filename);
    printf("Documented symbols: %d\n", mod->symbolCount);

    for (i = 0; i < mod->symbolCount; i++) {
        const Symbol *sym = &mod->symbols[i];

        printf("\n[%d] %s\n", i + 1, or_null(sym->name));
        printf("    Name       : %s\n", or_null(sym->name));
        printf("    Description: %s\n", or_null(sym->description));
        printf("    Signature  : %s\n", or_null(sym->signature));
        printf("    Line       : %u\n", sym->line);
        printf("    Output     : %s\n", or_null(sym->output));
        printf("    Notes      : %s\n", or_null(sym->notes));
        printf("    Type       : %s\n", or_null(sym->type));
        printf("    Diagram    : %s\n", or_null(sym->diagram));
        printf("    Input (%d)  :", sym->inputCount);
        if (sym->inputCount == 0) {
            printf(" (none)\n");
        } else {
            printf("\n");
            for (j = 0; j < sym->inputCount; j++)
                printf("      - %s - %s\n", or_null(sym->input[j].name),
                       or_null(sym->input[j].description));
        }
    }
}