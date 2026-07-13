/*
 * Reconstructs directory/file paths from a DxModel's parent-index links -
 * the same idea as doc_extractor's own modtree_file_path, done here
 * independently since the renderer only has the already-built DxModel, not
 * the module_tree tables doc_extractor built it from.
 */
#include "md_renderer.h"

#include <stdio.h>
#include <string.h>

int md_dir_path(const DxModel *m, int dir_index, char *out, size_t out_size) {
    if(dir_index < 0) { if(out_size) out[0] = '\0'; return 0; }

    /* Collect the chain of indices from leaf to root, then print root-first. */
    int chain[256];
    int depth = 0;
    int idx = dir_index;
    while(idx >= 0 && depth < (int)(sizeof chain / sizeof *chain)) {
        chain[depth++] = idx;
        idx = m->dirs[idx].parent_index;
    }

    size_t used = 0;
    for(int k = depth - 1; k >= 0; k--) {
        const char *name = m->dirs[chain[k]].name ? m->dirs[chain[k]].name : "";
        size_t need = strlen(name) + (used > 0 ? 1 : 0);
        if(used + need >= out_size) return -1;
        if(used > 0) out[used++] = '/';
        memcpy(out + used, name, strlen(name));
        used += strlen(name);
    }
    out[used] = '\0';
    return 0;
}

int md_file_path(const DxModel *m, size_t file_index, char *out, size_t out_size) {
    const DxFile *f = &m->files[file_index];
    char dir_path[1024];
    if(md_dir_path(m, f->parent_dir_index, dir_path, sizeof dir_path) != 0) return -1;

    const char *name = f->name ? f->name : "";
    size_t need = strlen(dir_path) + (dir_path[0] ? 1 : 0) + strlen(name) + 1;
    if(need > out_size) return -1;

    if(dir_path[0]) {
        snprintf(out, out_size, "%s/%s", dir_path, name);
    } else {
        snprintf(out, out_size, "%s", name);
    }
    return 0;
}
