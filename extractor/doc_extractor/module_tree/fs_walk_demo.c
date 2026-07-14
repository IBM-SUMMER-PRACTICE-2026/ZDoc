#include "fs_walk.h"
#include "modtree_tables.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <folder> [ext1 ext2 ...]\n", argv[0]);
        fprintf(stderr, "  no extensions given -> matches every file\n");
        fprintf(stderr, "example: %s ./myproject .plx .pli\n", argv[0]);
        return 1;
    }

    const char* root_path = argv[1];

    /* Remaining argv entries (if any) become the extension filter list. */
    size_t extension_count = (argc > 2) ? (size_t)(argc - 2) : 0;
    const char** extensions = (extension_count > 0) ? (const char**)&argv[2] : NULL;

    modtree_dir_table_t dirs;
    modtree_file_table_t files;
    modtree_dir_table_init(&dirs);
    modtree_file_table_init(&files);

    enum ZDoc_Error rc = fs_walk(root_path, &dirs, &files, extensions, extension_count, NULL, 0, 1);
    if (rc != ZDOC_OK) {
        fprintf(stderr, "fs_walk failed on '%s' (error %d)\n", root_path, rc);
        modtree_dir_table_free(&dirs);
        modtree_file_table_free(&files);
        return 1;
    }

    printf("dir_table:\n");
    for (size_t i = 0; i < dirs.count; i++) {
        printf("  %zu: { name=\"%s\", parent=%d }\n", i, dirs.dirs[i].name, dirs.dirs[i].parent_index);
    }

    printf("file_table:\n");
    for (size_t i = 0; i < files.count; i++) {
        printf("  %zu: { name=\"%s\", parent_dir_index=%d }\n",
               i, files.files[i].name, files.files[i].parent_dir_index);
    }

    printf("\nreconstructed paths:\n");
    char path[1024];
    for (size_t i = 0; i < files.count; i++) {
        if (modtree_file_path(&dirs, &files, (int)i, path, sizeof(path)) == ZDOC_OK) {
            printf("  %s\n", path);
        } else {
            printf("  <path too long for file index %zu>\n", i);
        }
    }

    printf("\n%zu directories, %zu files\n", dirs.count, files.count);

    modtree_dir_table_free(&dirs);
    modtree_file_table_free(&files);
    return 0;
}