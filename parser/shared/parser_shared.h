#ifndef PARSER_SHARED_H
#define PARSER_SHARED_H

#include <stdlib.h>
#include <stdatomic.h>
#include "../../extractor/doc_extractor/module_tree/modtree_tables.h"

/* Structure for the global format of output of the parsers.
    (For now this is example name name
    and it is full of garbage just for the test.)
*/
typedef struct{
    int testVar;
} TempName;

extern _Atomic int finished_files;
extern modtree_dir_table_t global_dir_table;
extern modtree_file_table_t global_file_table;
extern int files_count;
extern TempName* global_parsed_files_arry;

/* Function that initializes the files count,
    getting it from the interface that modtree_tables.h provides
    and allocates memory for the global_parsed_files_arry.
    returns 0 on succsess
    returns -1 on failure
*/
int init_resources();

#endif
