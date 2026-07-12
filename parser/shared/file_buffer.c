/*
 * file_buffer — read a whole source file into a padded in-memory buffer.
 *
 * The buffer carries 16 trailing NUL bytes so parsers can look a few bytes
 * past the end of the content without an out-of-bounds read. Allocation and
 * release live entirely here; parsers deal only with FileBuffer.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file_buffer.h"

/* NUL padding bytes appended after the file contents. */
#define FILE_BUFFER_PADDING 16

FileBuffer read_file_buffer(const char *path) {
    FileBuffer fb = { NULL, 0 };
    FILE *f;
    long size;
    char *buf;
    size_t rd;

    f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "zdoc: %s: %s\n", path, strerror(errno));
        return fb;
    }

    fseek(f, 0, SEEK_END);
    size = ftell(f);
    if (size < 0) {
        fclose(f);
        fprintf(stderr, "zdoc: %s: unseekable file\n", path);
        return fb;
    }
    fseek(f, 0, SEEK_SET);

    /* Over-allocate so the scanner can read past the end safely. */
    buf = (char *)malloc((size_t)size + FILE_BUFFER_PADDING);
    if (!buf) {
        fclose(f);
        fprintf(stderr, "zdoc: %s: out of memory\n", path);
        return fb;
    }

    rd = fread(buf, 1, (size_t)size, f);
    fclose(f);
    memset(buf + rd, 0, FILE_BUFFER_PADDING); /* pad from the actual bytes read */

    fb.data = buf;
    fb.len = rd;
    return fb;
}

void free_file_buffer(FileBuffer *fb) {
    if (!fb)
        return;
    free(fb->data);
    fb->data = NULL;
    fb->len = 0;
}
