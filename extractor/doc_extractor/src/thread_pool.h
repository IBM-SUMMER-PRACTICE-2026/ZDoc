/*
 * Cross-platform chunk-sizing and thread pool used to run parser
 * invocations in parallel: pthreads on POSIX, native Win32 threads on
 * Windows - no sequential fallback on either platform.
 */
#ifndef ZDOC_THREAD_POOL_H
#define ZDOC_THREAD_POOL_H

#include <stddef.h>

#include "child_parser.h"
#include "doc_extractor.h"

/* A single unit of work: one chunk of files to run through one parser. */
typedef struct {
    const LangEntry  *lang;
    const char       *parser_dir;
    const char      **paths;
    DxFile          **targets;
    size_t            count;
} ChunkWork;

/* Number of usable cores/hardware threads on this machine (always >= 1). */
size_t chunk_pool_cpu_count(void);

/* Computes the chunk size to use across ALL language groups combined - see
 * thread_pool.c for why total_files must be the combined count, not one
 * group's count. */
size_t chunk_pool_compute_chunk_size(size_t total_files);

/* Runs every ChunkWork item in work[0..total_chunks) across a thread pool
 * sized to chunk_pool_cpu_count() (capped at total_chunks), then blocks
 * until all of them have completed. */
void chunk_pool_run(ChunkWork *work, size_t total_chunks);

#endif /* ZDOC_THREAD_POOL_H */
