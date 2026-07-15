#include "threading_interface.h"

#ifdef _WIN32

/**
 * @brief Win32 thread trampoline: adapt a void(void) callback to Win32's
 *        thread entry point signature.
 *
 * @param arg The void(void) function pointer to invoke, passed through as
 *            the Win32 thread parameter.
 * @return Always 0.
 */
static DWORD WINAPI thread_start(LPVOID arg) {
    void (*func)(void) = arg;
    func();
    return 0;
}

/**
 * @brief Start a new Win32 thread running thread_func.
 *
 * @param thread_func Function the new thread will run, via the
 *                     thread_start() trampoline.
 * @param out Receives the created thread handle on success.
 * @return ZDOC_OK on success, or ZDOC_THREAD_CREATE_FAILED if
 *         CreateThread() failed.
 */
enum ZDoc_Error create_thread(void (*thread_func)(void), type_thread* out) {
    HANDLE thread = CreateThread(
        NULL,
        0,
        thread_start,
        thread_func,
        0,
        NULL
    );

    if (thread == NULL) return ZDOC_THREAD_CREATE_FAILED;
    *out = thread;
    return ZDOC_OK;
}

/**
 * @brief Block until a Win32 thread finishes, then close its handle.
 *
 * @param working_thread Thread handle to wait on and close.
 * @return ZDOC_OK once the thread has finished, or
 *         ZDOC_THREAD_WAIT_FAILED if the wait itself failed.
 */
enum ZDoc_Error wait_for_thread(type_thread* working_thread) {
    DWORD result = WaitForSingleObject(*working_thread, INFINITE);
    CloseHandle(*working_thread);
    return (result == WAIT_FAILED) ? ZDOC_THREAD_WAIT_FAILED : ZDOC_OK;
}

#else

/**
 * @brief Start a new pthread running thread_func.
 *
 * @param thread_func Function the new thread will run.
 * @param out Receives the created pthread_t on success.
 * @return ZDOC_OK on success, or ZDOC_THREAD_CREATE_FAILED if
 *         pthread_create() failed.
 */
enum ZDoc_Error create_thread(void (*thread_func)(void), type_thread* out) {
    pthread_t thread;
    if (pthread_create(&thread, NULL, (void *(*)(void *))thread_func, NULL) != 0) {
        return ZDOC_THREAD_CREATE_FAILED;
    }
    *out = thread;
    return ZDOC_OK;
}

/**
 * @brief Block until a pthread finishes (joins it).
 *
 * @param working_thread Thread to join.
 * @return ZDOC_OK once the thread has finished, or
 *         ZDOC_THREAD_WAIT_FAILED if pthread_join() failed.
 */
enum ZDoc_Error wait_for_thread(type_thread* working_thread) {
    return (pthread_join(*working_thread, NULL) != 0) ? ZDOC_THREAD_WAIT_FAILED : ZDOC_OK;
}

#endif