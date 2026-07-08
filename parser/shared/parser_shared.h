#ifndef PARSER_SHARED_H
#define PARSER_SHARED_H

#include <stdlib.h>
#include <stdatomic.h>
#include "../../extractor/doc_extractor/module_tree/modtree_tables.h"


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
    char *output;
    char *notes;
    uint32_t line;
    char *type;
    char *diagram; // NULL until online mode
} Symbol;


// Module - the information for each file and the array of symbols for it.
typedef struct {
    char *filename;
    Symbol *symbols;
    int symbolCount;
    int symbolCap;
} Module;


extern _Atomic int finished_files;
extern modtree_dir_table_t global_dir_table;
extern modtree_file_table_t global_file_table;
extern int files_count;
extern Module* global_parsed_files_arry;

/* Function that initializes the files count,
    getting it from the interface that modtree_tables.h provides
    and allocates memory for the global_parsed_files_arry.
    returns 0 on succsess
    returns -1 on failure
*/

int init_resources();

#endif
