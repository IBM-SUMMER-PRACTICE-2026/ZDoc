#include "parser_shared.h"

int init_resources() {
    return number_of_files(&global_file_table);
}