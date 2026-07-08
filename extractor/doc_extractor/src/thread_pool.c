#include "thread_pool.h"
#include "json_read.h" /* xmalloc */

#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <pthread.h>
#endif

/* Chunk sizing constants.
 * MIN_CHUNK_SIZE: prevents tiny groups from being split into pointlessly
 *   small pieces (e.g. 5 PLX files should stay as one chunk, not five).
 * MAX_CHUNK_SIZE: bounds how many file arguments a single parser
 *   invocation can receive — limits argv length and the blast radius
 *   of a single invocation's failure. */
#define MIN_CHUNK_SIZE 8
#define MAX_CHUNK_SIZE 200

size_t chunk_pool_cpu_count(void) {
#ifdef _WIN32
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwNumberOfProcessors > 0 ? (size_t)info.dwNumberOfProcessors : 1;
#else
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (size_t)n : 1;
#endif
}

/* total_files here is the sum across every language group (Java + C + PLX +
 * ...), not one group's count - all groups share the same thread pool, so
 * sizing chunks off a single group's count (and independently trying to hit
 * "one chunk per core" per group) causes the total chunk count to multiply
 * with the number of languages in the repo: 3 language groups on an 18-core
 * box would each aim for ~18 chunks, i.e. 54 total, forcing three rounds
 * through an 18-wide pool instead of one. Sizing off the combined total
 * keeps the grand total near one chunk per core however many groups the
 * repo has, since a bigger group naturally gets more of the shared chunk
 * budget and a smaller one gets less.
 *
 * Ceiling-divide the combined total by the core count, so the sum of
 * per-group ceil(group_count / chunk_size) lands on (at most a couple more
 * than) `cores` chunks - enough to fill every core in a single round. Floor
 * division here would systematically produce one extra chunk per group
 * (from the remainder), each spilling into an extra thread-pool round that
 * only exists to cover a handful of leftover files - paying a full
 * spawn+parse latency for that round.
 *
 * Why one chunk per core (a single round) is provably optimal here, not
 * just a good guess - apply Amdahl's Law to this specific cost model:
 *
 *   Speedup(N) = 1 / ((1-P) + P/N)
 *
 * The "serial" cost Amdahl's Law charges you for is the per-round spawn
 * overhead: a round runs its chunks fully in parallel, but rounds
 * themselves stack sequentially, so K rounds always pay K * spawn_overhead
 * no matter how many cores you have. The parallelizable cost is parse
 * time, which does divide across min(cores, chunk_count) workers within
 * a round. So for chunk_count C on `cores` cores:
 *
 *   Total(C) = ceil(C/cores) * spawn_overhead
 *            + ceil(C/cores) * (total_parse_time / C)
 *
 * Within any single-round bracket (C <= cores) this is minimized by
 * taking C as large as possible, i.e. C = cores (parse time keeps
 * shrinking, spawn_overhead is paid once regardless). Past that, every
 * additional round multiplies spawn_overhead by k while only dividing
 * total_parse_time by a slightly larger C - a strictly worse trade, since
 * spawn_overhead > 0. Solving Total(C) at the top of each round-bracket
 * (C = k*cores) gives Total = k*spawn_overhead + total_parse_time/cores,
 * which is monotonically increasing in k. So the global minimum over all
 * valid C is always at k=1, C=cores: exactly the one-chunk-per-core,
 * single-round target this function computes. There is no smarter
 * chunk count to pick under this cost model - more chunks only buys
 * more rounds, and more rounds only buys more unrecoverable serial
 * overhead. */
size_t chunk_pool_compute_chunk_size(size_t total_files) {
    size_t cores = chunk_pool_cpu_count();
    if(!cores) cores = 1;
    size_t size = (total_files + cores - 1) / cores;
    if(size < MIN_CHUNK_SIZE) size = MIN_CHUNK_SIZE;
    if(size > MAX_CHUNK_SIZE) size = MAX_CHUNK_SIZE;
    return size;
}

/* Thin cross-platform wrappers so the pool below is written once: pthreads
 * on POSIX, native Win32 threads/critical sections on Windows. */
#ifdef _WIN32
typedef CRITICAL_SECTION dx_mutex_t;
typedef HANDLE           dx_thread_t;
static void dx_mutex_init(dx_mutex_t *m)    { InitializeCriticalSection(m); }
static void dx_mutex_lock(dx_mutex_t *m)    { EnterCriticalSection(m); }
static void dx_mutex_unlock(dx_mutex_t *m)  { LeaveCriticalSection(m); }
static void dx_mutex_destroy(dx_mutex_t *m) { DeleteCriticalSection(m); }
#else
typedef pthread_mutex_t dx_mutex_t;
typedef pthread_t       dx_thread_t;
static void dx_mutex_init(dx_mutex_t *m)    { pthread_mutex_init(m, NULL); }
static void dx_mutex_lock(dx_mutex_t *m)    { pthread_mutex_lock(m); }
static void dx_mutex_unlock(dx_mutex_t *m)  { pthread_mutex_unlock(m); }
static void dx_mutex_destroy(dx_mutex_t *m) { pthread_mutex_destroy(m); }
#endif

/* Shared state for the worker threads: a flat array of ChunkWork items
 * and a mutex-protected counter for the next item to pick up. */
typedef struct {
    ChunkWork *items;
    size_t     total;
    size_t     next;      /* index of the next unclaimed item */
    dx_mutex_t mutex;
} WorkQueue;

/* Runs on every worker thread (either kind): pull chunks off the shared
 * queue until it's empty, spawning the parser for each one. */
static void run_queue(WorkQueue *q) {
    for(;;) {
        dx_mutex_lock(&q->mutex);
        size_t idx = q->next;
        if(idx < q->total) q->next++;
        dx_mutex_unlock(&q->mutex);

        if(idx >= q->total) break; /* no more work */

        ChunkWork *w = &q->items[idx];
        run_parser_batch(w->lang, w->parser_dir, w->paths, w->targets, w->count);
    }
}

#ifdef _WIN32
static DWORD WINAPI worker_thread(LPVOID arg) {
    run_queue((WorkQueue *)arg);
    return 0;
}
#else
static void *worker_thread(void *arg) {
    run_queue((WorkQueue *)arg);
    return NULL;
}
#endif

void chunk_pool_run(ChunkWork *work, size_t total_chunks) {
    WorkQueue q;
    q.items = work;
    q.total = total_chunks;
    q.next  = 0;
    dx_mutex_init(&q.mutex);

    size_t n_threads = chunk_pool_cpu_count();
    /* Don't spawn more threads than chunks — pointless overhead. */
    if(n_threads > total_chunks) n_threads = total_chunks;

    dx_thread_t *threads = xmalloc(n_threads * sizeof(dx_thread_t));
#ifdef _WIN32
    for(size_t t = 0; t < n_threads; t++)
        threads[t] = CreateThread(NULL, 0, worker_thread, &q, 0, NULL);
    for(size_t t = 0; t < n_threads; t++) {
        WaitForSingleObject(threads[t], INFINITE);
        CloseHandle(threads[t]);
    }
#else
    for(size_t t = 0; t < n_threads; t++)
        pthread_create(&threads[t], NULL, worker_thread, &q);
    for(size_t t = 0; t < n_threads; t++)
        pthread_join(threads[t], NULL);
#endif
    free(threads);

    dx_mutex_destroy(&q.mutex);
}
