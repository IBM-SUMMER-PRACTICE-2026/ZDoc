#include "process_interface.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32

/**
 * @brief Spawn bob_cli as a detached Win32 process wired to the given
 *        pipe ends.
 *
 * Runs bob_cli through "cmd.exe /c" with "-o text -y" (one-shot,
 * auto-approved, plain-text output), redirecting its stdin/stdout to
 * in_Rd/out_Wd. Closes the process and thread handles immediately since
 * the caller only communicates through the pipes, and closes its own
 * copies of in_Rd/out_Wd once the child has inherited them.
 *
 * @param bob_cli Command used to invoke the Bob CLI.
 * @param in_Rd Read end of the input pipe, wired to the child's stdin.
 * @param in_Wd Write end of the input pipe (unused here; caller keeps it).
 * @param out_Rd Read end of the output pipe (unused here; caller keeps it).
 * @param out_Wd Write end of the output pipe, wired to the child's stdout.
 */
void create_process(const char* bob_cli, h in_Rd, h in_Wd, h out_Rd, h out_Wd) {
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput  = in_Rd;
    si.hStdOutput = out_Wd;
    si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);

    char cmd[1024];
    snprintf(cmd, sizeof cmd, "cmd.exe /c \"%s\" -o text -y", bob_cli);

    CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    CloseHandle(in_Rd);
    CloseHandle(out_Wd);
}

/**
 * @brief Create the stdin/stdout pipe pair for a Bob subprocess and start
 *        it via create_process().
 *
 * Marks the parent's own ends (in_Wd, out_Rd) as non-inheritable so only
 * the child's dup'd handles carry the pipes across CreateProcessA().
 *
 * @param bob_cli Command used to invoke the Bob CLI.
 * @param in_Rd Receives the input pipe's read end (child's stdin).
 * @param in_Wd Receives the input pipe's write end (parent writes here).
 * @param out_Rd Receives the output pipe's read end (parent reads here).
 * @param out_Wd Receives the output pipe's write end (child's stdout).
 */
void open_pipes_bob_comunication(const char* bob_cli, h* in_Rd, h* in_Wd, h* out_Rd, h* out_Wd) {
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };

    CreatePipe(in_Rd, in_Wd, &sa, 0);
    SetHandleInformation(*in_Wd, HANDLE_FLAG_INHERIT, 0);

    CreatePipe(out_Rd, out_Wd, &sa, 0);
    SetHandleInformation(*out_Rd, HANDLE_FLAG_INHERIT, 0);

    create_process(bob_cli, *in_Rd, *in_Wd, *out_Rd, *out_Wd);
}

/**
 * @brief Write the full prompt to the child's stdin, then close it.
 *
 * Closing in_Wd after the write signals EOF on the child's stdin, which
 * is how a one-shot Bob CLI invocation knows the prompt is complete and
 * it should start answering. Stops early (leaving a partial write) if a
 * WriteFile call fails or writes zero bytes.
 *
 * @param in_Wd Write end of the input pipe; closed by this call.
 * @param data Prompt bytes to write.
 * @param len Number of bytes in data.
 */
void bob_write_message(h in_Wd, const char* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        DWORD written = 0;
        if (!WriteFile(in_Wd, data + sent, (DWORD)(len - sent), &written, NULL) || written == 0) {
            break;
        }
        sent += written;
    }

    CloseHandle(in_Wd);
}

/**
 * @brief Build the prompt for module and send it on in_Wd.
 *
 * Thin convenience wrapper combining build_bob_prompt() and
 * bob_write_message() for callers that don't need the two steps split.
 *
 * @param in_Wd Write end of the input pipe; closed by bob_write_message().
 * @param module Module the prompt is built for.
 * @param prompt Receives the heap-allocated prompt string; caller must
 *               free() it.
 * @param path Path of the source file Bob should read.
 */
void bob_send_prompt(h in_Wd, Module* module, char** prompt, const char* path) {
    build_bob_prompt(path, module, prompt);
    bob_write_message(in_Wd, *prompt, strlen(*prompt));
}

/**
 * @brief Read the child's entire stdout into a heap-allocated, growable
 *        buffer.
 *
 * Blocks in a ReadFile loop until the pipe reports no more data, which
 * happens once the child process exits and its stdout handle closes.
 * Doubles the buffer whenever it fills up.
 *
 * @param out_Rd Read end of the output pipe; closed by this call.
 * @return NUL-terminated heap-allocated response the caller must free(),
 *         or NULL on allocation failure.
 */
char* read_bob_message(h out_Rd) {
    size_t cap = 4096, len = 0;
    char* buf = malloc(cap);
    if (buf == NULL) {
        CloseHandle(out_Rd);
        return NULL;
    }

    DWORD bytesRead = 0;
    while (ReadFile(out_Rd, buf + len, (DWORD)(cap - len - 1), &bytesRead, NULL) && bytesRead > 0) {
        len += bytesRead;
        if (len + 1 >= cap) {
            cap *= 2;
            char* grown = realloc(buf, cap);
            if (grown == NULL) {
                free(buf);
                CloseHandle(out_Rd);
                return NULL;
            }
            buf = grown;
        }
    }
    buf[len] = '\0';
    CloseHandle(out_Rd);

    return buf; // caller must free()
}

#else

/**
 * @brief Fork and exec bob_cli, wiring the given pipe ends to its
 *        stdin/stdout.
 *
 * Runs bob_cli through execlp() with "-o text -y" (one-shot,
 * auto-approved, plain-text output). The child dup2()s in_Rd/out_Wd onto
 * its stdin/stdout and closes all four raw pipe fds before exec; the
 * parent closes its own copies of in_Rd/out_Wd once the child owns them.
 * If execlp() itself fails, the child exits immediately via _exit(127)
 * rather than returning into the caller's code.
 *
 * @param bob_cli Command used to invoke the Bob CLI.
 * @param in_Rd Read end of the input pipe, dup2'd onto the child's stdin.
 * @param in_Wd Write end of the input pipe (unused here; caller keeps it).
 * @param out_Rd Read end of the output pipe (unused here; caller keeps it).
 * @param out_Wd Write end of the output pipe, dup2'd onto the child's
 *               stdout.
 */
void create_process(const char* bob_cli, h in_Rd, h in_Wd, h out_Rd, h out_Wd) {
    pid_t pid = fork();

    if (pid == 0) {
        dup2(in_Rd, STDIN_FILENO);
        dup2(out_Wd, STDOUT_FILENO);

        close(in_Rd);
        close(in_Wd);
        close(out_Rd);
        close(out_Wd);

        execlp(bob_cli, bob_cli, "-o", "text", "-y", (char*)NULL);
        _exit(127);
    }

    close(in_Rd);
    close(out_Wd);
}

/**
 * @brief Create the stdin/stdout pipe pair for a Bob subprocess and start
 *        it via create_process().
 *
 * @param bob_cli Command used to invoke the Bob CLI.
 * @param in_Rd Receives the input pipe's read end (child's stdin).
 * @param in_Wd Receives the input pipe's write end (parent writes here).
 * @param out_Rd Receives the output pipe's read end (parent reads here).
 * @param out_Wd Receives the output pipe's write end (child's stdout).
 */
void open_pipes_bob_comunication(const char* bob_cli, h* in_Rd, h* in_Wd, h* out_Rd, h* out_Wd) {
    int in_fds[2];
    int out_fds[2];

    pipe(in_fds);
    pipe(out_fds);

    *in_Rd  = in_fds[0];
    *in_Wd  = in_fds[1];
    *out_Rd = out_fds[0];
    *out_Wd = out_fds[1];

    create_process(bob_cli, *in_Rd, *in_Wd, *out_Rd, *out_Wd);
}

/**
 * @brief Write the full prompt to the child's stdin, then close it.
 *
 * Closing in_Wd after the write signals EOF on the child's stdin, which
 * is how a one-shot Bob CLI invocation knows the prompt is complete and
 * it should start answering. Stops early (leaving a partial write) if a
 * write() call fails.
 *
 * @param in_Wd Write end of the input pipe; closed by this call.
 * @param data Prompt bytes to write.
 * @param len Number of bytes in data.
 */
void bob_write_message(h in_Wd, const char* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t written = write(in_Wd, data + sent, len - sent);
        if (written <= 0) {
            break;
        }
        sent += (size_t)written;
    }

    close(in_Wd);
}

/**
 * @brief Build the prompt for module and send it on in_Wd.
 *
 * Thin convenience wrapper combining build_bob_prompt() and
 * bob_write_message() for callers that don't need the two steps split.
 *
 * @param in_Wd Write end of the input pipe; closed by bob_write_message().
 * @param module Module the prompt is built for.
 * @param prompt Receives the heap-allocated prompt string; caller must
 *               free() it.
 * @param path Path of the source file Bob should read.
 */
void bob_send_prompt(h in_Wd, Module* module, char** prompt, const char* path) {
    build_bob_prompt(path, module, prompt);
    bob_write_message(in_Wd, *prompt, strlen(*prompt));
}

/**
 * @brief Read the child's entire stdout into a heap-allocated, growable
 *        buffer.
 *
 * Blocks in a read() loop until it returns 0, which happens once the
 * child process exits and its stdout fd closes. Doubles the buffer
 * whenever it fills up.
 *
 * @param out_Rd Read end of the output pipe; closed by this call.
 * @return NUL-terminated heap-allocated response the caller must free(),
 *         or NULL on allocation failure.
 */
char* read_bob_message(h out_Rd) {
    size_t cap = 4096, len = 0;
    char* buf = malloc(cap);
    if (buf == NULL) {
        close(out_Rd);
        return NULL;
    }

    ssize_t bytesRead;
    while ((bytesRead = read(out_Rd, buf + len, cap - len - 1)) > 0) {
        len += (size_t)bytesRead;
        if (len + 1 >= cap) {
            cap *= 2;
            char* grown = realloc(buf, cap);
            if (grown == NULL) {
                free(buf);
                close(out_Rd);
                return NULL;
            }
            buf = grown;
        }
    }
    buf[len] = '\0';
    close(out_Rd);

    return buf; // caller must free()
}

#endif
