#include "path_interface.h"

#ifdef _WIN32

#include <windows.h>

/**
 * @brief Resolve input to an absolute, normalized path (Windows back end).
 *
 * Delegates to GetFullPathNameA, which collapses "." and ".." segments and
 * redundant separators.
 *
 * @param input Path to resolve, absolute or relative.
 * @param out Caller-provided buffer that receives the resolved path.
 * @param out_size Size in bytes of out.
 * @return 0 on success, -1 if resolution failed or out was too small.
 */
int resolve_absolute_path(const char* input, char* out, size_t out_size) {
    DWORD n = GetFullPathNameA(input, (DWORD)out_size, out, NULL);
    return (n == 0 || n >= out_size) ? -1 : 0;
}

#else

#include <stdlib.h>

/**
 * @brief Resolve input to an absolute, normalized path (POSIX back end).
 *
 * Delegates to realpath, which also collapses symlinks along the way.
 *
 * @param input Path to resolve, absolute or relative.
 * @param out Caller-provided buffer that receives the resolved path; must be
 *            at least PATH_MAX bytes, since realpath fills up to that size.
 * @param out_size Unused on this back end - realpath has no length limit
 *                 parameter and out is assumed sized for it.
 * @return 0 on success, -1 on failure.
 */
int resolve_absolute_path(const char* input, char* out, size_t out_size) {
    (void)out_size;   /* realpath fills up to PATH_MAX; out is sized for it */
    return realpath(input, out) ? 0 : -1;
}

#endif
