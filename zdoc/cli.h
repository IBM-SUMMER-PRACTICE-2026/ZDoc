#ifndef ZD_CLI_H
#define ZD_CLI_H

#include "options.h"

typedef enum {
    ZD_CLI_OK = 0,
    ZD_CLI_EXIT,
    ZD_CLI_ERROR
} zd_cli_result;

zd_cli_result zd_cli_parse(int argc, char **argv, zd_options *o);

#endif