/* main.c - zdoc CLI entry point: defaults -> zdoc.yaml -> argv -> request. */
#include "options.h"
#include "config.h"
#include "cli.h"
#include "request.h"
#include "../zdoc_daemon.h"
#include <stdlib.h>

/**
 * @brief zdoc CLI entry point.
 *
 * Resolves options in precedence order: built-in defaults
 * (zd_options_init), then ./zdoc.yaml or ./zdoc.json (zd_config_load),
 * then command-line arguments (zd_cli_parse), then validates the
 * resolved source paths (zd_request_validate) before handing off to
 * zdoc_daemon_start_job() to run the actual walk/parse/render job.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return 0 on success or on --help/--version/no-args exit; 1 if request
 *         validation failed; 2 on a CLI parse error; 3 if the daemon job
 *         failed; 4 if zd_options_init() could not allocate its option
 *         storage.
 */
int main(int argc, char **argv) {
    zd_options opt;
    zd_cli_result rc;

    if (zd_options_init(&opt) != ZDOC_OK) return 4;
    zd_config_load(&opt);
    rc = zd_cli_parse(argc, argv, &opt);
    if(rc == ZD_CLI_EXIT) return 0;
    if(rc == ZD_CLI_ERROR) return 2;

    if(zd_request_validate(&opt) != ZDOC_OK) return 1;

    /* Placeholder for the daemon call: emit the request it will receive. */
    // zd_request_write(&opt, stdout);

    if (zdoc_daemon_start_job(&opt) != ZDOC_OK) return 3;

    return 0;
}
