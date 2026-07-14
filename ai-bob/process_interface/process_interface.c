#include "process_interface.h"
#include <stdlib.h>

#ifdef _WIN32

void create_process(const char* bob_cli, h in_Rd, h in_Wd, h out_Rd, h out_Wd) {
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput  = in_Rd;
    si.hStdOutput = out_Wd;
    si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);

    char cmd[1024];
    snprintf(cmd, sizeof cmd, "\"%s\" -o text -y", bob_cli);

    CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    CloseHandle(in_Rd);
    CloseHandle(out_Wd);
}

void open_pipes_bob_comunication(const char* bob_cli, h* in_Rd, h* in_Wd, h* out_Rd, h* out_Wd) {
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };

    CreatePipe(in_Rd, in_Wd, &sa, 0);
    SetHandleInformation(*in_Wd, HANDLE_FLAG_INHERIT, 0);

    CreatePipe(out_Rd, out_Wd, &sa, 0);
    SetHandleInformation(*out_Rd, HANDLE_FLAG_INHERIT, 0);

    create_process(bob_cli, *in_Rd, *in_Wd, *out_Rd, *out_Wd);
}

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

void bob_send_prompt(h in_Wd, Module* module, char** prompt, const char* path) {
    build_bob_prompt(path, module, prompt);
    bob_write_message(in_Wd, *prompt, strlen(*prompt));
}

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

void bob_send_prompt(h in_Wd, Module* module, char** prompt, const char* path) {
    build_bob_prompt(path, module, prompt);
    bob_write_message(in_Wd, *prompt, strlen(*prompt));
}

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
