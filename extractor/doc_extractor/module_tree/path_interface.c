#include "path_interface.h"

#ifdef _WIN32

#include <windows.h>

int resolve_absolute_path(const char* input, char* out, size_t out_size) {
    DWORD n = GetFullPathNameA(input, (DWORD)out_size, out, NULL);
    return (n == 0 || n >= out_size) ? -1 : 0;
}

#else

#include <stdlib.h>

int resolve_absolute_path(const char* input, char* out, size_t out_size) {
    (void)out_size;   /* realpath fills up to PATH_MAX; out is sized for it */
    return realpath(input, out) ? 0 : -1;
}

#endif
