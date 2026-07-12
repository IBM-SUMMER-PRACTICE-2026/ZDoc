#ifndef FILE_BUFFER_H
#define FILE_BUFFER_H

#include <stddef.h>

/* A file read fully into memory: data holds the file's bytes followed by 16
 * NUL padding bytes, so a scanner can safely read past the end of the content.
 * len is the number of real bytes read, excluding the padding. On a failed
 * read data is NULL and len is 0. */
typedef struct {
    char *data;
    size_t len;
} FileBuffer;

/* Read the whole file at path into an over-allocated, NUL-padded buffer. On
 * failure prints a "zdoc:" diagnostic to stderr and returns { NULL, 0 }. */
FileBuffer read_file_buffer(const char *path);

/* Release a buffer obtained from read_file_buffer and reset the struct. */
void free_file_buffer(FileBuffer *fb);

#endif
