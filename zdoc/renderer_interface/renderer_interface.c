#include "renderer_interface.h"

void render(const char* out_dir, const char* title, zd_format format) {
    switch (format)
    {
    case ZD_FORMAT_MD:
        md_render(&global_dir_table, &global_file_table,
             global_parsed_files_arry, files_count, out_dir, title);
        break;
    case ZD_FORMAT_HTML:
        html_render(&global_dir_table, &global_file_table,
            global_parsed_files_arry, files_count, out_dir, title);
    default:
        break;
    }
}