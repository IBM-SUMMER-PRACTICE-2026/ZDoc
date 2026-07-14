/* Process-spawn seam: the one place bob_client touches the OS.
 *
 * bob_client.c above this seam is portable — it only builds an argv vector and
 * sanitizes captured text. Everything OS-specific (POSIX fork/exec vs. Windows
 * CreateProcess) lives behind bc_spawn_capture, with a separate implementation
 * file per platform (spawn_posix.c / spawn_win.c) selected by the Makefile.
 *
 * The snippet reaches the child as a single argv element on every platform:
 * on POSIX execvp takes the argv array verbatim (no quoting, ever); on Windows
 * the impl rebuilds a correctly-quoted command line per the CommandLineToArgvW
 * rules, so arbitrary source — quotes, backslashes, newlines — round-trips
 * without injection.
 */
#ifndef ZDOC_BC_SPAWN_H
#define ZDOC_BC_SPAWN_H

#include <stddef.h>

/* Spawn argv[0] with the NULL-terminated argv vector, capturing its stdout.
 *
 * On success returns 0, sets *out to a malloc'd, NUL-terminated buffer of the
 * child's stdout (caller frees; never NULL, "" if the child printed nothing),
 * and writes the child's exit code to *exit_code (a program that could not be
 * started exits 127, matching a POSIX shell). Returns -1 only when the child
 * could not be spawned or its output could not be captured; *out and
 * *exit_code are then left untouched.
 */
int bc_spawn_capture(char *const argv[], char **out, int *exit_code);

#endif /* ZDOC_BC_SPAWN_H */
