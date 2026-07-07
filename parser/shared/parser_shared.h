#ifndef PARSER_SHARED_H
#define PARSER_SHARED_H

#include <stdatomic.h>
#include "../../extractor/doc_extractor/module_tree/modtree_tables.h"

extern _Atomic int finished_files = 0;
extern modtree_dir_table_t global_dir_table;
extern modtree_file_table_t global_file_table;
extern int files_count = 0;

/* Function that initializes the filres count,
    getting it from the interface that modtree_tables.h provides.
    returns 0 on succsess
    returns -1 on failure
*/
int init_resources();

#endif