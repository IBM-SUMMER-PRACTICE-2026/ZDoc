#ifndef BOB_CLIENT_H
#define BOB_CLIENT_H

#include "../zdoc/error_interface.h"
#include "../parser/parser_interface.h"

// Orchestrates the bob logic
ZDoc_Error bob_client(const char * path, Module * module);

#endif