#include "../parser/shared/parser_shared.h"
#include "../extractor/doc_extractor/module_tree/fs_walk.h"
#include "../extractor/doc_extractor/module_tree/modtree_tables.h"
#include "../parser/plx_parser/plx_parser.h"
#include "../parser/parser_interface.h"
#include "./threading_interface/threading_interface.h"
#include <stdio.h>
#include <string.h>

const char* root_path;
size_t extension_count;
const char** extensions;

void thread_func() {
    int curr_possition_in_arry;
    char* path = malloc(sizeof(char) * 4096);

    while(finished_files != files_count) {
        curr_possition_in_arry = finished_files;
        finished_files++;
        enum Language lang = language_from_name(global_file_table.files[curr_possition_in_arry].name);
        if ((int)lang < 0) {
            continue;
        }

        modtree_file_path(&global_dir_table, &global_file_table, curr_possition_in_arry, path, 4096);

        Module* finished = parse_file(lang, path);
        if (finished == NULL) {
            continue;
        }

        global_parsed_files_arry[curr_possition_in_arry] = *finished;
    }

    free(path);
}

int main(int argc, char* argv[]) {
    
    if (argc < 2) {
        fprintf(stderr, "usage: %s <folder> [ext1 ext2 ...]\n", argv[0]);
        fprintf(stderr, "  no extensions given -> matches every file\n");
        fprintf(stderr, "example: %s ./myproject .plx .pli\n", argv[0]);
        return 1;
    }

    root_path = argv[1];

    extension_count = (argc > 2) ? (size_t)(argc - 2) : 0;
    extensions = (extension_count > 0) ? (const char**)&argv[2] : NULL;

    modtree_dir_table_init(&global_dir_table);
    modtree_file_table_init(&global_file_table);

    int rc = fs_walk(root_path, &global_dir_table, &global_file_table, extensions, extension_count);
    if (rc != 0) {
        fprintf(stderr, "fs_walk failed on '%s'\n", root_path);
        modtree_dir_table_free(&global_dir_table);
        modtree_file_table_free(&global_file_table);
        return 1;
    }

    init_resources();

    type_thread thread = create_thread(thread_func);
    wait_for_thread(&thread);


    modtree_dir_table_free(&global_dir_table);
    modtree_file_table_free(&global_file_table);

}