#ifndef THREADING_INTERFACE_H
#define THREADING_INTERFACE_H

#ifdef _WIN32
    #include <windows.h>
    typedef HANDLE type_thread;
#else
    #include <pthread.h>
    typedef pthread_t type_thread;
#endif

type_thread create_thread(void (*thread_func)(void));
void wait_for_thread(type_thread* working_thread);

#endif