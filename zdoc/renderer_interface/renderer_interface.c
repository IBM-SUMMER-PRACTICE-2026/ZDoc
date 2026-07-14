#include "renderer_interface.h"

enum ZDoc_Error render(const char* out_dir, const char* title, zd_format format) {
    switch (format)
    {
    case ZD_FORMAT_MD:
        return md_render(&global_dir_table, &global_file_table,
             global_parsed_files_arry, files_count, out_dir, title);
    case ZD_FORMAT_HTML:
        return html_render(&global_dir_table, &global_file_table,
            global_parsed_files_arry, files_count, out_dir, title);
    default:
        return ZDOC_UNSUPPORTED_FORMAT;
    }
}