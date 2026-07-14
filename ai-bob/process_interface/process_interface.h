#ifndef PROCESS_INTERFACE
#define PROCESS_INTERFACE

#include "../bob-client.h"
#include <string.h>

#ifdef _WIN32

#include <windows.h>
typedef HANDLE h;

#else

#include <unistd.h>
#include <sys/wait.h>

typedef int h;

#endif

void open_pipes_bob_comunication(const char* bob_cli, h* in_Rd, h* in_Wd, h* out_Rd, h* out_Wd);
void create_process(const char* bob_cli, h in_Rd, h in_Wd, h out_Rd, h out_Wd);
void bob_send_prompt(h in_Wd, Module* module, char** prompt, const char* path);

void bob_write_message(h in_Wd, const char* data, size_t len);

char* read_bob_message(h out_Rd);

#endif