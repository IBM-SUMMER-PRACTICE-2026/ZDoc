/* request.h — formatting the resolved options as a zdoc daemon request. */
#ifndef ZD_REQUEST_H
#define ZD_REQUEST_H

#include <stdio.h>

#include "options.h"

/* Checks that every source path exists. Returns 0 if all do. */
int zd_request_validate(const zd_options *o);

/* Writes the generate-request JSON to out. Until the daemon's transport
 * is decided, main() sends it to stdout; later a daemon client module
 * will ship this same payload over the wire. */
void zd_request_write(const zd_options *o, FILE *out);

#endif /* ZD_REQUEST_H */
