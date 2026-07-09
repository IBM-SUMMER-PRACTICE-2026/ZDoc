/* main.c - zdoc CLI entry point: defaults -> zdoc.yaml -> argv -> request. */
#include "options.h"
#include "config.h"
#include "cli.h"
#include "request.h"

int main(int argc, char **argv) {
    zd_options opt;
    zd_cli_result rc;

    zd_options_init(&opt);
    zd_config_load(&opt);
    rc = zd_cli_parse(argc, argv, &opt);
    if(rc == ZD_CLI_EXIT) return 0;
    if(rc == ZD_CLI_ERROR) return 2;

    if(zd_request_validate(&opt) != 0) return 1;

    /* Placeholder for the daemon call: emit the request it will receive. */
    zd_request_write(&opt, stdout);
    return 0;
}
