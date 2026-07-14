/* POSIX implementation of the bc_spawn_capture seam (see spawn.h).
 *
 * fork + pipe + execvp + waitpid: the child's stdout is redirected down a pipe
 * the parent drains to EOF, then reaps the child for its exit status. Passing
 * the snippet as a single argv element means no shell and no quoting — source
 * with quotes, newlines, backslashes cannot inject a command. Compiled on
 * Linux and macOS (and POSIX emulation layers such as Cygwin); Windows uses
 * spawn_win.c instead.
 */
#if !defined(_WIN32)

#define _POSIX_C_SOURCE 200809L

#include "spawn.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define READ_CHUNK 4096

int bc_spawn_capture(char *const argv[], char **out_buf, int *exit_code)
{
    int fds[2];
    if (pipe(fds) != 0)
        return -1;

    pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return -1;
    }

    if (pid == 0) {
        /* Child: send stdout down the pipe, then exec. */
        dup2(fds[1], STDOUT_FILENO);
        close(fds[0]);
        close(fds[1]);
        execvp(argv[0], argv);
        _exit(127); /* exec failed (e.g. program not on PATH) */
    }

    /* Parent: read the child's stdout to EOF. */
    close(fds[1]);

    char *out = NULL;
    size_t len = 0, cap = 0;
    int ok = 1;
    ssize_t r;
    char buf[READ_CHUNK];
    while ((r = read(fds[0], buf, sizeof(buf))) > 0) {
        if (len + (size_t)r + 1 > cap) {
            size_t ncap = cap ? cap * 2 : READ_CHUNK * 2;
            while (ncap < len + (size_t)r + 1)
                ncap *= 2;
            char *grown = realloc(out, ncap);
            if (!grown) {
                ok = 0;
                break;
            }
            out = grown;
            cap = ncap;
        }
        memcpy(out + len, buf, (size_t)r);
        len += (size_t)r;
    }
    if (r < 0)
        ok = 0;
    close(fds[0]);

    int status = 0;
    waitpid(pid, &status, 0);

    if (!ok) {
        free(out);
        return -1;
    }
    if (!out) {
        out = malloc(1); /* child produced nothing; return "" */
        if (!out)
            return -1;
    }
    out[len] = '\0';

    *out_buf = out;
    *exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return 0;
}

#endif /* !_WIN32 */
