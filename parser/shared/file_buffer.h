#ifndef FILE_BUFFER_H
#define FILE_BUFFER_H

#include <stddef.h>

/* A file mapped into memory: data points to exactly len bytes with no trailing
 * padding, so consumers must bounds-check every access against len. On a failed
 * read data is NULL and len is 0. */
typedef struct {
    char *data;
    size_t len;
} FileBuffer;

/* Memory-map the whole file at path read-only. On failure prints a "zdoc:"
 * diagnostic to stderr and returns { NULL, 0 }. */
FileBuffer read_file_buffer(const char *path);

/* Release a buffer obtained from read_file_buffer and reset the struct. */
void free_file_buffer(FileBuffer *fb);

#endif
