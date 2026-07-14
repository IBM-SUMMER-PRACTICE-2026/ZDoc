/*
 * file_buffer — map a whole source file into memory.
 *
 * The file is memory-mapped read-only, exactly its own length with no trailing
 * padding; parsers must bounds-check every access against fb->len. Mapping and
 * release live entirely here; parsers deal only with FileBuffer.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "file_buffer.h"

#if defined(_WIN32)

#include <windows.h>

#else

#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#endif


/**
 * @brief Memory-map an entire file into memory read-only.
 *
 * On Windows this opens the file, maps it with CreateFileMapping/
 * MapViewOfFile, and closes both the file and mapping handles once the view
 * is established (the view itself keeps the data alive until
 * free_file_buffer() is called). On other platforms it opens the file, stats
 * it, and mmap()s it PROT_READ/MAP_PRIVATE, closing the descriptor
 * immediately after mapping. An empty file (size 0) is treated the same as a
 * failure and yields a zeroed buffer. On any failure a "zdoc:" diagnostic is
 * printed to stderr.
 *
 * @param path Path of the file to map.
 * @return A FileBuffer pointing at the mapped data with its length set, or
 *         { NULL, 0 } if the file could not be opened, sized, mapped, or is
 *         empty.
 */
FileBuffer read_file_buffer(const char *path) {
    FileBuffer fb = { NULL, 0 };
#if defined(_WIN32)
    HANDLE hFile = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    if (hFile == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "zdoc: %s: couldn't open file\n", path);
        return fb;
    }

    LARGE_INTEGER size;
    if (!GetFileSizeEx(hFile, &size)) {
        fprintf(stderr, "zdoc: %s: couldn't get file size\n", path);
        CloseHandle(hFile);
        return fb;
    }

    if (size.QuadPart == 0) {
        CloseHandle(hFile);
        return fb;
    }

    HANDLE hMap = CreateFileMapping(
        hFile,
        NULL,
        PAGE_READONLY,
        0,
        0,
        NULL
    );
    if (!hMap) {
        fprintf(stderr, "zdoc: %s: couldn't map file\n", path);
        CloseHandle(hFile);
        return fb;
    }

    fb.data = (char *)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    /* The view keeps the file and mapping alive, so both handles can be
       released now; the view itself is freed by UnmapViewOfFile. */
    CloseHandle(hMap);
    CloseHandle(hFile);
    if (!fb.data) {
        fprintf(stderr, "zdoc: %s: couldn't map view of file\n", path);
        return fb;
    }
    fb.len = (size_t)size.QuadPart;
#else
    int fd = open(path, O_RDONLY);
    if(fd == -1) {
        fprintf(stderr, "zdoc: %s: couldn't open file\n", path);
        return fb;
    }
    struct stat sb;

    if(fstat(fd, &sb) == -1) {
        fprintf(stderr, "zdoc: %s: couldn't get file size\n", path);
        close(fd);
        return fb;
    }

    if(sb.st_size == 0) {
        close(fd);
        return fb;
    }

    fb.data = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if(fb.data == MAP_FAILED) {
        fb.data = NULL;
        close(fd);
        return fb;
    }
    fb.len = sb.st_size;
    close(fd);
#endif
    return fb;
}

/**
 * @brief Release a buffer obtained from read_file_buffer() and reset it.
 *
 * Unmaps the underlying memory (UnmapViewOfFile on Windows, munmap()
 * elsewhere) and zeroes out fb->data and fb->len. Safe to call with a NULL
 * pointer or a buffer whose data is already NULL, in which case it is a
 * no-op.
 *
 * @param fb The FileBuffer to release.
 */
void free_file_buffer(FileBuffer *fb) {
    if (!fb || !fb->data)
        return;

#if defined(_WIN32)
    UnmapViewOfFile(fb->data);
#else
    munmap(fb->data, fb->len);
#endif
    fb->data = NULL;
    fb->len = 0;
}
