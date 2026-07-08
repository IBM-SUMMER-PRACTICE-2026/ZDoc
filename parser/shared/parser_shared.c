#include "parser_shared.h"

_Atomic int finished_files = 0;
modtree_dir_table_t global_dir_table;
modtree_file_table_t global_file_table;
int files_count = 0;
Module* global_parsed_files_arry = NULL;

int init_resources() {
    int count = number_of_files(&global_file_table);
    if (count < 0) return -1;
    files_count = count;

    global_parsed_files_arry = malloc(sizeof(Module) * files_count);

    return global_parsed_files_arry ? 0 : -1;
}
