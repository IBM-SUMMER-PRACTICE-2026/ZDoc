#include "threading_interface.h"

#ifdef _WIN32

static DWORD WINAPI thread_start(LPVOID arg) {
    void (*func)(void) = arg;
    func();
    return 0;
}

type_thread create_thread(void (*thread_func)(void)) {
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

void wait_for_thread(type_thread* working_thread) {
    WaitForSingleObject(*working_thread, INFINITE);
    CloseHandle(*working_thread);
}

#else

type_thread create_thread(void (*thread_func)(void)) {
    pthread_t thread;
    pthread_create(&thread, NULL, (void *(*)(void *))thread_func, NULL);
    return thread;
}

void wait_for_thread(type_thread* working_thread) {
    pthread_join(*working_thread, NULL);
}

#endif