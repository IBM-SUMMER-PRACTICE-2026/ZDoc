#include <stdio.h>
#include <assert.h>
#include <stdatomic.h>
#include "parser_shared.h"

/* Test for the shared global parser resources.
 *
 * Builds a mock module-tree file table containing a single file, publishes
 * it as the global file table, runs init_resources(), then asserts that every
 * shared global ends up in the expected state.
 */
int main(void) {
    /* --- Arrange: a mock file table holding exactly one file --- */
    modtree_file_table_init(&global_file_table);
    int idx = modtree_intern_file(&global_file_table, "mock_file.plx", -1);
    assert(idx == 0);                          /* first file gets index 0 */
    assert(global_file_table.count == 1);      /* table really has one file */

    /* --- Act: run the code under test --- */
    enum ZDoc_Error rc = init_resources();

    /* --- Assert: all globals are correct --- */
    assert(rc == ZDOC_OK);                     /* init reported success */
    assert(files_count == 1);                  /* count picked up from the table */
    assert(global_parsed_files_arry != NULL);  /* output array was allocated */
    assert(atomic_load(&finished_files) == 0); /* nothing parsed yet */

    printf("All global resource checks passed.\n");

    /* --- Cleanup --- */
    free(global_parsed_files_arry);
    modtree_file_table_free(&global_file_table);
    return 0;
}
