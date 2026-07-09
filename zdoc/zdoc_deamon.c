#include "../parser/shared/parser_shared.h"
#include "../extractor/doc_extractor/module_tree/fs_walk.h"
#include "../extractor/doc_extractor/module_tree/modtree_tables.h"
#include "../parser/plx_parser/plx_parser.h"
#include "../parser/parser_interface.h"
#include "./threading_interface/threading_interface.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdatomic.h>

#ifndef NUM_THREADS
#define NUM_THREADS 4
#endif

const char* root_path;
size_t extension_count;
const char** extensions;

static double now_seconds(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
#endif
}

void thread_func() {
    for (;;) {
        int curr_possition_in_arry = atomic_fetch_add(&finished_files, 1);
        if (curr_possition_in_arry >= files_count) {
            break;
        }
        enum Language lang = language_from_name(global_file_table.files[curr_possition_in_arry].name);
        if ((int)lang < 0) {
            continue;
        }

        char* path = paths_look_up[curr_possition_in_arry];
        int prefix_len = snprintf(path, 4096, "%s/", fs_walk_root_prefix);
        if (prefix_len < 0 || prefix_len >= 4096) {
            continue;
        }
        modtree_file_path(&global_dir_table, &global_file_table,
                          curr_possition_in_arry, path + prefix_len, 4096 - prefix_len);

        char* shrunk = realloc(path, strlen(path) + 1);
        if (shrunk != NULL) {
            path = shrunk;
            paths_look_up[curr_possition_in_arry] = shrunk;
        }

        Module* finished = parse_file(lang, path);
        if (finished == NULL) {
            continue;
        }

        finished->pathIndex = curr_possition_in_arry;
        global_parsed_files_arry[curr_possition_in_arry] = *finished;
    }
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

    double start = now_seconds();

    type_thread threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        threads[i] = create_thread(thread_func);
    }
    for (int i = 0; i < NUM_THREADS; i++) {
        wait_for_thread(&threads[i]);
    }

    double elapsed = now_seconds() - start;
    printf("%d threads parsed %d files in %.3f s\n",
           NUM_THREADS, files_count, elapsed);

    modtree_dir_table_free(&global_dir_table);
    modtree_file_table_free(&global_file_table);

}