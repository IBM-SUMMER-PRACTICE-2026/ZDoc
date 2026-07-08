#include "../parser/shared/parser_shared.h"
#include "../extractor/doc_extractor/module_tree/fs_walk.h"
#include "../extractor/doc_extractor/module_tree/modtree_tables.h"
#include "../parser/plx_parser/plx_parser.h"
#include "../parser/parser_interface.h"
#include <stdio.h>
#include <string.h>

const char* root_path;
size_t extension_count;
const char** extensions;

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

    // prints for the paths
    /*
    printf("\nreconstructed paths:\n");
    char path[1024];
    for (size_t i = 0; i < global_file_table.count; i++) {
        if (modtree_file_path(&global_dir_table, &global_file_table, (int)i, path, sizeof(path)) == 0) {
            printf("  %s\n", path);
        } else {
            printf("  <path too long for file index %zu>\n", i);
        }
    }
    */

    init_resources();
    char* path = malloc(sizeof(char) * 4096);

    for(int i = 0; i < files_count; i++) {
        enum Language lang = language_from_name(global_file_table.files[i].name);
        if ((int)lang < 0) {
            continue;
        }

        modtree_file_path(&global_dir_table, &global_file_table, i, path, 4096);

        Module* finished = parse_file(lang, path);
        if (finished == NULL) {
            continue;
        }

        global_parsed_files_arry[finished_files++] = *finished;
    }

    free(path);

    modtree_dir_table_free(&global_dir_table);
    modtree_file_table_free(&global_file_table);
 
}