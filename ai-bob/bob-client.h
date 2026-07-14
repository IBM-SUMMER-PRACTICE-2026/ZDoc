#ifndef BOB_CLIENT_H
#define BOB_CLIENT_H

#include "../zdoc/error_interface.h"
#include "../parser/parser_interface.h"

// Orchestrates the bob logic
enum ZDoc_Error bob_client(const char * path, Module * module, char * bob_cli);

// Builds the entire prompt for Bob (instructions, path + function names + line numbers)
enum ZDoc_Error build_bob_prompt(const char * path, Module * module, char ** prompt);

// Makes the call to Bob CLI
enum ZDoc_Error bob_call(const char * prompt, char * response, size_t * response_len, char * bob_cli);

// Staple a diagram to its symbol
enum ZDoc_Error append_diagram_to_symbol(const char * diagram, size_t diagram_len, Symbol * symbol);

// Staple each diagram to its symbol (calls append_diagram for each symbol)
enum ZDoc_Error append_diagrams(const char * response, size_t response_len, Module * module);

// Builds the entire prompt for Bob (instructions, path + function names + line numbers)
char * build_bob_prompt(const char * path, Module * module);

#endif