#include "renderer_interface.h"

/**
 * @brief Dispatch to the renderer matching format.
 *
 * Renders the parsed modules currently held in the global directory/file
 * tables and global_parsed_files_arry (populated by the daemon's parse
 * pass) to out_dir, in either Markdown (md_render) or HTML (html_render).
 *
 * @param out_dir Root output directory to write the rendered pages to.
 * @param title Project title shown in the rendered output.
 * @param format Which renderer to invoke.
 * @return Whatever the chosen renderer returns, or
 *         ZDOC_UNSUPPORTED_FORMAT if format matches neither case.
 */
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