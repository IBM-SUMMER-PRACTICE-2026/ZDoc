#ifndef BOB_CLIENT_H
#define BOB_CLIENT_H

#include "../zdoc/error_interface.h"
#include "../parser/parser_interface.h"

// Orchestrates the bob logic
ZDoc_Error bob_client(const char * path, Module * module);

// Builds the entire prompt for Bob (instructions, path + function names + line numbers)
char * build_bob_prompt(const char * path, Module * module);

// Makes the call to Bob CLI
ZDoc_Error bob_call(const char * prompt, char ** response);

#endif