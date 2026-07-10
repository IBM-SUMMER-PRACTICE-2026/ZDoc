#ifndef PARSER_SHARED_H
#define PARSER_SHARED_H

#include <stdlib.h>
#include <stdint.h>
#include <stdatomic.h>
#include "../../extractor/doc_extractor/module_tree/modtree_tables.h"


extern _Atomic int finished_files;
extern modtree_dir_table_t global_dir_table;
extern modtree_file_table_t global_file_table;
extern int files_count;
extern struct Module* global_parsed_files_arry;


// Function input parameter
typedef struct {
    char *name;
    char *description;
} InputParam;


// Parsed informaton for each symbol.
typedef struct {
    char *name;
    char *description;
    char *signature;
    InputParam *input;
    int inputCount;
    int inputCap;
    char *output;
    char *notes;
    uint32_t line;
    char *type;
    char *diagram; // NULL until online mode
} Symbol;


// Module - the information for each file and the array of symbols for it.
typedef struct Module {
    char *filename;
    Symbol *symbols;
    int symbolCount;
    int symbolCap;
} Module;

/* Function that initializes the files count,
    getting it from the interface that modtree_tables.h provides
    and allocates memory for the global_parsed_files_arry.
    returns 0 on succsess
    returns -1 on failure
*/

int init_resources();


/*********************************************/
/*             INPUT PARAMS                  */
/*********************************************/
void free_input_param_content(InputParam *param);



/*********************************************/
/*                 SYMBOL                    */
/*********************************************/
void symbol_add_input(Symbol *sym, const char *name, const char *description);
void symbol_shrink_inputs_to_fit(Symbol *sym);
void free_symbol_content(Symbol * sym);



/*********************************************/
/*                MODULE                     */
/*********************************************/
Module * init_module(const char *path);
Symbol *module_add_symbol(Module *mod);
void module_shrink_to_fit(Module *mod);
void free_module(Module *mod);



/*********************************************/
/*                 OUTPUT                    */
/*********************************************/
void print_module(const Module *mod);



void *xmalloc(size_t n);
void *xrealloc(void *p, size_t n);
char *xstrdup(const char *s);
char *xstrndup(const char *s, size_t n);


#endif
