#include "threading_interface.h"

#ifdef _WIN32

static DWORD WINAPI thread_start(LPVOID arg) {
    void (*func)(void) = arg;
    func();
    return 0;
}

thread_handle_t create_thread(void (*thread_func)(void)) {
    HANDLE thread = CreateThread(
        NULL,
        0,
        thread_start,
        thread_func,
        0,
        NULL
    );

    return thread;
}

#else

type_thread create_thread(void (*thread_func)(void)) {
    pthread_t thread;
    pthread_create(&thread, NULL, &thread_func, NULL);
    return thread;
}

#endif