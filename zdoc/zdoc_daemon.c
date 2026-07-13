#include "../parser/shared/parser_shared.h"
#include "../extractor/doc_extractor/module_tree/fs_walk.h"
#include "../extractor/doc_extractor/module_tree/modtree_tables.h"
#include "../parser/plx_parser/plx_parser.h"
#include "../parser/parser_interface.h"
#include "./threading_interface/threading_interface.h"
#include "zdoc_daemon.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdatomic.h>

#ifndef NUM_THREADS
#define NUM_THREADS 12
#endif

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

        free(finished);
    }
}

void zdoc_daemon_start_job(zd_options* options) {

    modtree_dir_table_init(&global_dir_table);
    modtree_file_table_init(&global_file_table);

    int rc = fs_walk(options->inputs[0], &global_dir_table, &global_file_table,
                      (const char**)options->languages, options->n_languages);
    if (rc != 0) {
        fprintf(stderr, "fs_walk failed on '%s'\n", options->inputs[0]);
        modtree_dir_table_free(&global_dir_table);
        modtree_file_table_free(&global_file_table);
        return;
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