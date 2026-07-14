#include "parser_shared.h"

_Atomic int finished_files = 0;
modtree_dir_table_t global_dir_table;
modtree_file_table_t global_file_table;
int files_count = 0;
struct Module* global_parsed_files_arry = NULL;
char** paths_look_up = NULL;

int init_resources() {
    int count = number_of_files(&global_file_table);
    if (count < 0) return -1;
    files_count = count;

    global_parsed_files_arry = malloc(sizeof(Module) * files_count);
    if (global_parsed_files_arry == NULL) return -1;

    paths_look_up = malloc(sizeof(char*) * files_count);
    if (paths_look_up == NULL) return -1;

    for (int i = 0; i < files_count; i++) {
        paths_look_up[i] = malloc(sizeof(char) * 4096);
        if (paths_look_up[i] == NULL) return -1;
    }

    return 0;
}
