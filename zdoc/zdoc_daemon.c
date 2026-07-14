#include "../parser/shared/parser_shared.h"
#include "../extractor/doc_extractor/module_tree/fs_walk.h"
#include "../extractor/doc_extractor/module_tree/modtree_tables.h"
#include "../parser/plx_parser/plx_parser.h"
#include "../parser/parser_interface.h"
#include "./threading_interface/threading_interface.h"
#include "./renderer_interface/renderer_interface.h"
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

static Module set_NULL_on_fail(enum ZDoc_Error error) {
    Module m = {.filename=NULL, .pathIndex = -1, .symbolCap=0, .symbolCount=0, .symbols = NULL, .status = error};
    return m;
}

void thread_func() {
    for (;;) {
        int curr_possition_in_arry = atomic_fetch_add(&finished_files, 1);
        if (curr_possition_in_arry >= files_count) {
            break;
        }
        enum Language lang;
        if (language_from_name(global_file_table.files[curr_possition_in_arry].name, &lang) == ZDOC_UNSUPPORTED_LANGUAGE) {
            global_parsed_files_arry[curr_possition_in_arry] = set_NULL_on_fail(ZDOC_UNSUPPORTED_LANGUAGE);
            continue;
        }

        char* path = paths_look_up[curr_possition_in_arry];
        int prefix_len = snprintf(path, 4096, "%s/", fs_walk_root_prefix);
        if (prefix_len < 0 || prefix_len >= 4096) {
            global_parsed_files_arry[curr_possition_in_arry] = set_NULL_on_fail(ZDOC_PATH_TOO_LONG);
            continue;
        }
        enum ZDoc_Error path_status = modtree_file_path(&global_dir_table, &global_file_table,
                          curr_possition_in_arry, path + prefix_len, 4096 - prefix_len);
        if (path_status != ZDOC_OK) {
            global_parsed_files_arry[curr_possition_in_arry] = set_NULL_on_fail(path_status);
            continue;
        }

        char* shrunk = realloc(path, strlen(path) + 1);
        if (shrunk != NULL) {
            path = shrunk;
            paths_look_up[curr_possition_in_arry] = shrunk;
        }

        Module* finished = parse_file(lang, path);
        if (finished == NULL) {
            global_parsed_files_arry[curr_possition_in_arry] = set_NULL_on_fail(ZDOC_PARSER_FAILED);
            continue;
        }

        finished->pathIndex = curr_possition_in_arry;
        global_parsed_files_arry[curr_possition_in_arry] = *finished;

        free(finished);
    }
}

enum ZDoc_Error zdoc_daemon_start_job(zd_options* options) {

    modtree_dir_table_init(&global_dir_table);
    modtree_file_table_init(&global_file_table);

    enum ZDoc_Error rc = fs_walk(options->inputs[0], &global_dir_table, &global_file_table,
                      (const char**)options->languages, options->n_languages,
                      (const char**)options->excludes, options->n_excludes,
                      options->recursive);
    if (rc != ZDOC_OK) {
        fprintf(stderr, "fs_walk failed on '%s' (error %d)\n", options->inputs[0], rc);
        modtree_dir_table_free(&global_dir_table);
        modtree_file_table_free(&global_file_table);
        return rc;
    }

    enum ZDoc_Error init_status = init_resources();
    if (init_status != ZDOC_OK) {
        fprintf(stderr, "init_resources failed (error %d)\n", init_status);
        modtree_dir_table_free(&global_dir_table);
        modtree_file_table_free(&global_file_table);
        return init_status;
    }

    double start = now_seconds();

    type_thread threads[NUM_THREADS];
    int threads_started = 0;
    enum ZDoc_Error thread_status = ZDOC_OK;
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_status = create_thread(thread_func, &threads[i]);
        if (thread_status != ZDOC_OK) break;
        threads_started++;
    }

    for (int i = 0; i < threads_started; i++) {
        enum ZDoc_Error wait_status = wait_for_thread(&threads[i]);
        if (thread_status == ZDOC_OK) thread_status = wait_status;
    }
    if (thread_status != ZDOC_OK) {
        fprintf(stderr, "thread pool failed (error %d)\n", thread_status);
        modtree_dir_table_free(&global_dir_table);
        modtree_file_table_free(&global_file_table);
        return thread_status;
    }

    enum ZDoc_Error render_status = render(options->out_dir, options->title, options->format);
    if (render_status != ZDOC_OK) {
        fprintf(stderr, "render failed (error %d)\n", render_status);
        modtree_dir_table_free(&global_dir_table);
        modtree_file_table_free(&global_file_table);
        return render_status;
    }

    double elapsed = now_seconds() - start;
    printf("%d threads parsed %d files and rendered them in %.10f s\n",
           NUM_THREADS, files_count, elapsed);

    modtree_dir_table_free(&global_dir_table);
    modtree_file_table_free(&global_file_table);

    return ZDOC_OK;
}