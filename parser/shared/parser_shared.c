#include "parser_shared.h"

_Atomic int finished_files = 0;
modtree_dir_table_t global_dir_table;
modtree_file_table_t global_file_table;
int files_count = 0;
struct Module* global_parsed_files_arry = NULL;
char** paths_look_up = NULL;

enum ZDoc_Error init_resources() {
    int count = number_of_files(&global_file_table);
    if (count < 0) return ZDOC_FILE_COUNT_UNAVAILABLE;
    files_count = count;

    global_parsed_files_arry = malloc(sizeof(Module) * files_count);
    if (global_parsed_files_arry == NULL) return ZDOC_OUT_OF_MEMORY;

    paths_look_up = malloc(sizeof(char*) * files_count);
    if (paths_look_up == NULL) return ZDOC_OUT_OF_MEMORY;

    for (int i = 0; i < files_count; i++) {
        paths_look_up[i] = malloc(sizeof(char) * 4096);
        if (paths_look_up[i] == NULL) return ZDOC_OUT_OF_MEMORY;
    }

    return ZDOC_OK;
}
