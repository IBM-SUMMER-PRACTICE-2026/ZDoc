#ifndef PATH_INTERFACE_H
#define PATH_INTERFACE_H

#include <stddef.h>

/* Resolve `input` to an absolute, normalized path written into `out`
 * (a caller-provided buffer of `out_size` bytes; on POSIX it must be at
 * least PATH_MAX). Symlinks, ".", ".." and redundant separators are
 * collapsed. Returns 0 on success, -1 on failure.
 *
 * Platform back ends: realpath() on POSIX, GetFullPathNameA() on Windows. */
int resolve_absolute_path(const char* input, char* out, size_t out_size);

#endif
