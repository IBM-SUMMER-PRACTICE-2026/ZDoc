/* Windows implementation of the bc_spawn_capture seam (see spawn.h).
 *
 * CreateProcess takes a single command-line *string*, not an argv array, so the
 * one hard part POSIX gets for free is rebuilding that string with correct
 * quoting — otherwise a snippet containing quotes, spaces or backslashes would
 * break the child's argv (or, worse, inject arguments). quote_arg below follows
 * the exact CommandLineToArgvW rules (see MSDN, "Parsing C++ Command-Line
 * Arguments"), so the child sees byte-identical argv elements to the POSIX path.
 *
 * stdout is captured through an inheritable pipe drained to EOF; the child's
 * stdin/stderr are left at the parent's. Compiled only for native Windows
 * (MSVC / MinGW); POSIX platforms use spawn_posix.c.
 */
#if defined(_WIN32)

#include "spawn.h"

#include <windows.h>
#include <stdlib.h>
#include <string.h>

#define READ_CHUNK 4096

/* Push one char onto a growable char buffer. Returns 0 on success, -1 on OOM. */
static int sb_putc(char **sb, size_t *len, size_t *cap, char c)
{
    if (*len + 1 > *cap) {
        size_t ncap = *cap ? *cap * 2 : 256;
        char *g = realloc(*sb, ncap);
        if (!g)
            return -1;
        *sb = g;
        *cap = ncap;
    }
    (*sb)[(*len)++] = c;
    return 0;
}

/* Append `arg` to the command line quoted per the CommandLineToArgvW rules. */
static int quote_arg(char **sb, size_t *len, size_t *cap, const char *arg)
{
    /* Bare append when the arg is non-empty and free of the characters that
     * would otherwise need quoting. */
    int needs = (arg[0] == '\0');
    for (const char *p = arg; *p && !needs; p++)
        if (*p == ' ' || *p == '\t' || *p == '"')
            needs = 1;

    if (!needs) {
        for (const char *p = arg; *p; p++)
            if (sb_putc(sb, len, cap, *p) != 0)
                return -1;
        return 0;
    }

    if (sb_putc(sb, len, cap, '"') != 0)
        return -1;
    for (const char *p = arg;; p++) {
        size_t nbs = 0;
        while (*p == '\\') {
            p++;
            nbs++;
        }
        if (*p == '\0') {
            /* Escape all trailing backslashes so they don't escape the
             * closing quote. */
            for (size_t i = 0; i < nbs * 2; i++)
                if (sb_putc(sb, len, cap, '\\') != 0)
                    return -1;
            break;
        } else if (*p == '"') {
            /* Double the backslashes, then escape the embedded quote. */
            for (size_t i = 0; i < nbs * 2 + 1; i++)
                if (sb_putc(sb, len, cap, '\\') != 0)
                    return -1;
            if (sb_putc(sb, len, cap, '"') != 0)
                return -1;
        } else {
            for (size_t i = 0; i < nbs; i++)
                if (sb_putc(sb, len, cap, '\\') != 0)
                    return -1;
            if (sb_putc(sb, len, cap, *p) != 0)
                return -1;
        }
    }
    return sb_putc(sb, len, cap, '"');
}

/* Build a NUL-terminated command line from argv. Caller frees. NULL on OOM. */
static char *build_cmdline(char *const argv[])
{
    char *sb = NULL;
    size_t len = 0, cap = 0;
    for (size_t i = 0; argv[i]; i++) {
        if (i && sb_putc(&sb, &len, &cap, ' ') != 0) {
            free(sb);
            return NULL;
        }
        if (quote_arg(&sb, &len, &cap, argv[i]) != 0) {
            free(sb);
            return NULL;
        }
    }
    if (sb_putc(&sb, &len, &cap, '\0') != 0) {
        free(sb);
        return NULL;
    }
    return sb;
}

int bc_spawn_capture(char *const argv[], char **out_buf, int *exit_code)
{
    char *cmdline = build_cmdline(argv);
    if (!cmdline)
        return -1;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof sa;
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE rd = NULL, wr = NULL;
    if (!CreatePipe(&rd, &wr, &sa, 0)) {
        free(cmdline);
        return -1;
    }
    /* The read end must not be inherited by the child. */
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = wr;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof pi);

    BOOL ok = CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL,
                             &si, &pi);
    free(cmdline);
    CloseHandle(wr); /* parent never writes; child owns its copy */

    if (!ok) {
        CloseHandle(rd);
        /* Could not start the program: mirror POSIX exec-fail (exit 127). */
        char *empty = malloc(1);
        if (!empty)
            return -1;
        empty[0] = '\0';
        *out_buf = empty;
        *exit_code = 127;
        return 0;
    }

    char *out = NULL;
    size_t len = 0, cap = 0;
    int good = 1;
    for (;;) {
        char buf[READ_CHUNK];
        DWORD got = 0;
        if (!ReadFile(rd, buf, sizeof buf, &got, NULL) || got == 0)
            break; /* broken pipe / EOF */
        if (len + got + 1 > cap) {
            size_t ncap = cap ? cap * 2 : READ_CHUNK * 2;
            while (ncap < len + got + 1)
                ncap *= 2;
            char *grown = realloc(out, ncap);
            if (!grown) {
                good = 0;
                break;
            }
            out = grown;
            cap = ncap;
        }
        memcpy(out + len, buf, got);
        len += got;
    }
    CloseHandle(rd);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (!good) {
        free(out);
        return -1;
    }
    if (!out) {
        out = malloc(1);
        if (!out)
            return -1;
    }
    out[len] = '\0';

    *out_buf = out;
    *exit_code = (int)code;
    return 0;
}

#endif /* _WIN32 */
