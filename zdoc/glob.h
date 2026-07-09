#ifndef ZDOC_GLOB_H
#define ZDOC_GLOB_H

/* Match `path` against a shell-style glob `pattern`. Rules:
 *   ?   any single character except '/'
 *   *   any run of characters except '/'
 *   **  any run of characters including '/'  (crosses directories)
 *   /   and every other character: literal
 * Matching is anchored (the whole path must match the whole pattern) and
 * case-sensitive. Returns 1 on match, 0 otherwise. */
int zdoc_glob_match(const char *pattern, const char *path);

#endif
