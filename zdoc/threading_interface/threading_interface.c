#include "threading_interface.h"

#ifdef _WIN32

static DWORD WINAPI thread_start(LPVOID arg) {
    void (*func)(void) = arg;
    func();
    return 0;
}

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

enum ZDoc_Error wait_for_thread(type_thread* working_thread) {
    DWORD result = WaitForSingleObject(*working_thread, INFINITE);
    CloseHandle(*working_thread);
    return (result == WAIT_FAILED) ? ZDOC_THREAD_WAIT_FAILED : ZDOC_OK;
}

#else

enum ZDoc_Error create_thread(void (*thread_func)(void), type_thread* out) {
    pthread_t thread;
    if (pthread_create(&thread, NULL, (void *(*)(void *))thread_func, NULL) != 0) {
        return ZDOC_THREAD_CREATE_FAILED;
    }
    *out = thread;
    return ZDOC_OK;
}

enum ZDoc_Error wait_for_thread(type_thread* working_thread) {
    return (pthread_join(*working_thread, NULL) != 0) ? ZDOC_THREAD_WAIT_FAILED : ZDOC_OK;
}

#endif