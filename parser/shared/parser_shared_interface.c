#include "parser_shared.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief Render a possibly-NULL string as "(null)" so every field prints.
 *
 * @param s The string to render, or NULL.
 * @return s itself if non-NULL, otherwise the literal "(null)".
 */
static const char *or_null(const char *s)
{
    return s ? s : "(null)";
}

/**
 * @brief Allocate memory, aborting the process on failure.
 *
 * Wraps malloc(); if the allocation fails, prints a "zdoc: out of memory"
 * diagnostic to stderr and terminates the process via exit(1) instead of
 * returning NULL.
 *
 * @param n Number of bytes to allocate.
 * @return Pointer to the newly allocated, uninitialised memory.
 */
void *xmalloc(size_t n)
{
    void *p = malloc(n);
    if (!p) {
        fprintf(stderr, "zdoc: out of memory\n");
        exit(1);
    }
    return p;
}

/**
 * @brief Resize a previously allocated block, aborting the process on failure.
 *
 * Wraps realloc(); if the reallocation fails, prints a "zdoc: out of memory"
 * diagnostic to stderr and terminates the process via exit(1) instead of
 * returning NULL.
 *
 * @param p Pointer to the block to resize, or NULL to allocate a new block.
 * @param n New size in bytes.
 * @return Pointer to the resized (possibly relocated) memory block.
 */
void *xrealloc(void *p, size_t n)
{
    p = realloc(p, n);
    if (!p) {
        fprintf(stderr, "zdoc: out of memory\n");
        exit(1);
    }
    return p;
}

/**
 * @brief Duplicate a NUL-terminated string, aborting the process on failure.
 *
 * Allocates strlen(s) + 1 bytes via xmalloc() and copies s, including its
 * terminator, into the new buffer. The caller owns the returned string and
 * must free() it.
 *
 * @param s The NUL-terminated string to duplicate.
 * @return A newly allocated copy of s.
 */
char *xstrdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = xmalloc(n);
    memcpy(p, s, n);
    return p;
}

/**
 * @brief Duplicate the first n bytes of a string into a NUL-terminated copy.
 *
 * Allocates n + 1 bytes via xmalloc(), copies n bytes from s, and appends a
 * NUL terminator. The caller owns the returned string and must free() it.
 *
 * @param s The string to copy from.
 * @param n Number of bytes to copy from s.
 * @return A newly allocated, NUL-terminated copy of the first n bytes of s.
 */
char *xstrndup(const char *s, size_t n)
{
    char *p = xmalloc(n + 1);
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

/**
 * @brief Get the last path component of a file path.
 *
 * Scans path for the last '/' or '\\' separator and returns a pointer just
 * past it; if no separator is found, returns path unchanged. The returned
 * pointer aliases into path and must not be freed separately.
 *
 * @param path The file path to strip.
 * @return Pointer into path at the start of its final component.
 */
const char *base_filename(const char *path)
{
    const char *base = path;
    const char *p;
    for (p = path; *p; p++) {
        if (*p == '/' || *p == '\\')
            base = p + 1;
    }
    return base;
}



/*********************************************/
/*             INPUT PARAMS                  */
/*********************************************/
/**
 * @brief Free the heap fields owned by an InputParam.
 *
 * Frees param->name and param->description. Does not free param itself, so
 * this is safe to call on a stack-allocated or array-embedded InputParam.
 *
 * @param param The InputParam whose name/description to free.
 */
void free_input_param_content(InputParam *param) {
    free(param->name);
    free(param->description);
}



/*********************************************/
/*                 SYMBOL                    */
/*********************************************/
/**
 * @brief Append an input parameter to a symbol's input list.
 *
 * Grows sym->input (doubling sym->inputCap, starting at 8) via xrealloc()
 * when the array is full, then stores newly xstrdup()'d copies of name and
 * description at the next slot and increments sym->inputCount. The Symbol
 * takes ownership of the duplicated strings.
 *
 * @param sym The symbol to append the input parameter to.
 * @param name Parameter name, copied into the symbol.
 * @param description Parameter description, copied into the symbol.
 */
void symbol_add_input(Symbol *sym, const char *name, const char *description) {
    if (sym->inputCount == sym->inputCap) {
        sym->inputCap = sym->inputCap ? sym->inputCap * 2 : 8;
        sym->input = xrealloc(sym->input,
                               (size_t)sym->inputCap * sizeof(InputParam));
    }
    sym->input[sym->inputCount].name = xstrdup(name);
    sym->input[sym->inputCount].description = xstrdup(description);
    sym->inputCount++;
}


/**
 * @brief Shrink a symbol's input array to exactly fit its element count.
 *
 * If inputCount equals inputCap, does nothing. Otherwise reallocates
 * sym->input down to sym->inputCount elements via xrealloc(), or frees it
 * and sets it to NULL if inputCount is 0. sym->inputCap is updated to match.
 *
 * @param sym The symbol whose input array to shrink.
 */
void symbol_shrink_inputs_to_fit(Symbol *sym)
{
    if (sym->inputCount == sym->inputCap)
        return;
    if (sym->inputCount == 0) {
        free(sym->input);
        sym->input = NULL;
    } else {
        sym->input = xrealloc(sym->input,
                              (size_t)sym->inputCount * sizeof(InputParam));
    }
    sym->inputCap = sym->inputCount;
}


/**
 * @brief Free every heap field owned by a Symbol, including its inputs.
 *
 * Frees name, description, signature, output, notes, type, and diagram,
 * frees the content of each InputParam in sym->input via
 * free_input_param_content(), and finally frees the input array itself.
 * Does not free sym itself.
 *
 * @param sym The symbol whose content to free.
 */
void free_symbol_content(Symbol * sym) {
    free(sym->name);
    free(sym->description);
    free(sym->signature);
    free(sym->output);
    free(sym->notes);
    free(sym->type);
    free(sym->diagram);
    for (int i = 0; i < sym->inputCount; i++) {
        free_input_param_content(&sym->input[i]);
    }
    free(sym->input);
}



/*********************************************/
/*                MODULE                     */
/*********************************************/
/**
 * @brief Allocate and initialise an empty Module for a source file.
 *
 * Allocates the Module via xmalloc(), sets filename to an xstrdup()'d copy
 * of base_filename(path) (so the Module owns its own filename, independent
 * of path's lifetime), and initialises the symbol array as empty with
 * status ZDOC_DEFAULT. pathIndex is left uninitialised.
 *
 * @param path Path of the source file this module represents; only its
 *             base name is retained.
 * @return A newly allocated, initialised Module. Never NULL (xmalloc aborts
 *         the process on allocation failure).
 */
Module * init_module(const char *path) {
    Module * mod = xmalloc(sizeof(Module));
    mod->filename = xstrdup(base_filename(path));
    mod->symbols = NULL;
    mod->symbolCount = 0;
    mod->symbolCap = 0;
    mod->status = ZDOC_DEFAULT;
    return mod;
}


/**
 * @brief Free a Module and everything it owns.
 *
 * Frees the content of every Symbol in mod->symbols via
 * free_symbol_content(), then frees the symbols array, the filename, and
 * finally mod itself. Safe to call with mod == NULL, in which case it is a
 * no-op.
 *
 * @param mod The module to free.
 */
void free_module(Module *mod)
{
    if (!mod)
        return;
    for (int i = 0; i < mod->symbolCount; i++) {
        free_symbol_content(&mod->symbols[i]);
    }
    free(mod->symbols);
    free(mod->filename);
    free(mod);
}


/**
 * @brief Append a new, zeroed Symbol slot to a module and return it.
 *
 * Grows mod->symbols (doubling mod->symbolCap, starting at 8) via
 * xrealloc() when the array is full, then zero-initialises the next slot,
 * increments mod->symbolCount, and returns a pointer to that slot for the
 * caller to populate.
 *
 * @param mod The module to append a symbol to.
 * @return Pointer to the newly added, zero-initialised Symbol.
 */
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


/**
 * @brief Shrink a module's symbol array to exactly fit its element count.
 *
 * If symbolCount equals symbolCap, does nothing. Otherwise reallocates
 * mod->symbols down to mod->symbolCount elements via xrealloc(), or frees
 * it and sets it to NULL if symbolCount is 0. mod->symbolCap is updated to
 * match.
 *
 * @param mod The module whose symbol array to shrink.
 */
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
/*                 OUTPUT                    */
/*********************************************/
/**
 * @brief Print a human-readable dump of a module and all its symbols.
 *
 * Writes the module's filename and symbol count to stdout, then for each
 * symbol prints its name, description, signature, line, output, notes,
 * type, diagram, and input parameter list; any NULL string field is printed
 * as "(null)" via or_null(), and an empty input list is printed as
 * "(none)".
 *
 * @param mod The module to print.
 */
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