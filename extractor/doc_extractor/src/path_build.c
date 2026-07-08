#include "path_build.h"

#include <string.h>
#include <stdio.h>

int build_disk_path(const modtree_dir_table_t *dirs, int dir_index,
                     const char *root_dir, const char *filename,
                     char *out, size_t out_size) {
    int chain[256];
    int depth = 0;
    int idx = dir_index;
    while(idx >= 0 && depth < (int)(sizeof chain / sizeof *chain)) {
        chain[depth++] = idx;
        idx = dirs->dirs[idx].parent_index;
    }

    int n = snprintf(out, out_size, "%s", root_dir);
    if(n < 0 || (size_t)n >= out_size) return -1;
    size_t used = (size_t)n;

    /* chain[depth-1] is the seeded root itself - skip it, only join the
     * segments strictly between root_dir and the file. */
    for(int k = depth - 2; k >= 0; k--) {
        const char *name = dirs->dirs[chain[k]].name ? dirs->dirs[chain[k]].name : "";
        size_t need = used + 1 + strlen(name);
        if(need >= out_size) return -1;
        out[used++] = '/';
        memcpy(out + used, name, strlen(name));
        used += strlen(name);
    }

    size_t need = used + 1 + strlen(filename);
    if(need >= out_size) return -1;
    out[used++] = '/';
    memcpy(out + used, filename, strlen(filename));
    used += strlen(filename);
    out[used] = '\0';
    return 0;
}
