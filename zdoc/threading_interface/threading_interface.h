#ifndef THREADING_INTERFACE_H
#define THREADING_INTERFACE_H

#include "../error_interface.h"

#ifdef _WIN32
    #include <windows.h>
    typedef HANDLE type_thread;
#else
    #include <pthread.h>
    typedef pthread_t type_thread;
#endif

enum ZDoc_Error create_thread(void (*thread_func)(void), type_thread* out);

enum ZDoc_Error wait_for_thread(type_thread* working_thread);

#endif