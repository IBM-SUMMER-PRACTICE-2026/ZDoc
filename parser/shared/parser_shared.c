#include "parser_shared.h"

int init_resources() {
    if (!number_of_files(&global_file_table)) return 0;
    else return -1;

    global_parsed_files_arry = malloc(sizeof(TempName) * files_count);
    
    return global_parsed_files_arry ? 0 : -1;
}