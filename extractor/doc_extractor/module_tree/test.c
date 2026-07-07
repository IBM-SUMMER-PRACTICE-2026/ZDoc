#include "modtree_tables.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

int main(void) {
    modtree_dir_table_t dirs;
    modtree_file_table_t files;
    modtree_dir_table_init(&dirs);
    modtree_file_table_init(&files);

    /* Build exactly:
     * folder1/
     *   folder2/
     *     file1.plx
     *     file2.plx
     *   folder3/
     *     file3.plx
     */
    int folder1 = modtree_intern_dir(&dirs, "folder1", -1);
    int folder2 = modtree_intern_dir(&dirs, "folder2", folder1);
    int folder3 = modtree_intern_dir(&dirs, "folder3", folder1);

    int file1 = modtree_intern_file(&files, "file1.plx", folder2);
    int file2 = modtree_intern_file(&files, "file2.plx", folder2);
    int file3 = modtree_intern_file(&files, "file3.plx", folder3);

    printf("dir_table:\n");
    for (size_t i = 0; i < dirs.count; i++) {
        printf("  %zu: { name=\"%s\", parent=%d }\n", i, dirs.dirs[i].name, dirs.dirs[i].parent_index);
    }

    printf("file_table:\n");
    for (size_t i = 0; i < files.count; i++) {
        printf("  %zu: { name=\"%s\", parent_dir_index=%d }\n",
               i, files.files[i].name, files.files[i].parent_dir_index);
    }

    char path[512];

    modtree_file_path(&dirs, &files, file1, path, sizeof(path));
    printf("file1 -> %s\n", path);
    assert(strcmp(path, "folder1/folder2/file1.plx") == 0);

    modtree_file_path(&dirs, &files, file2, path, sizeof(path));
    printf("file2 -> %s\n", path);
    assert(strcmp(path, "folder1/folder2/file2.plx") == 0);

    modtree_file_path(&dirs, &files, file3, path, sizeof(path));
    printf("file3 -> %s\n", path);
    assert(strcmp(path, "folder1/folder3/file3.plx") == 0);

    modtree_dir_path(&dirs, folder3, path, sizeof(path));
    printf("folder3 dir -> %s\n", path);
    assert(strcmp(path, "folder1/folder3") == 0);

    modtree_dir_table_free(&dirs);
    modtree_file_table_free(&files);

    printf("\nAll checks passed.\n");
    return 0;
}