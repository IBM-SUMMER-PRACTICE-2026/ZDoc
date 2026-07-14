#include <string.h>
#include <strings.h> //strncasecmp (POSIX but on MSVC its _strnicmp)

#include "doc_comment.h"
#include "util.h"

//Case-insensitive "does content start with word, followed by optional whitespace then ':'". On match, sets *value_start/*value_len to the trimmed text after the colon.
static int match_colon_label(const char *content, size_t clen, const char *word, const char **value_start, size_t *value_len) {
    size_t wlen = strlen(word);
    if(clen < wlen || strncasecmp(content, word, wlen) != 0) return 0;
    size_t p = wlen;
    while(p < clen && isspace((unsigned char)content[p])) p++;
    if(p >= clen || content[p] != ':') return 0;
    p++;
    trim(content + p, clen - p, value_start, value_len);
    return 1;
}

//Case-insensitive "does content start with @tag followed by a word boundary (whitespace, ':' or end of line)". On match, *rest_off is the offset right after the tag (not yet trimmed).
static int match_tag(const char *content, size_t clen, const char *tag, size_t *rest_off) {
    size_t tlen = strlen(tag);
    if(clen < tlen || strncasecmp(content, tag, tlen) != 0) return 0;
    if(clen > tlen && content[tlen] != ':' && !isspace((unsigned char)content[tlen])) return 0;
    *rest_off = tlen;
    return 1;
}

typedef enum {
    FIELD_NONE,
    FIELD_NAME,
    FIELD_BRIEF,
    FIELD_RETURNS,
    FIELD_NOTES,
    FIELD_PARAM
} Field;

int parse_doc_comment(const char *content, size_t clen, Symbol *out) {
    Buffer name_buf = {0}, brief_buf = {0}, returns_buf = {0}, notes_buf = {0};
    InputParam *params = NULL;
    Buffer *param_descs = NULL;
    size_t param_count = 0, param_cap = 0;
    size_t cur_param = 0;
    Field field = FIELD_NONE;
    int found_marker = 0;

    size_t line_start = 0;
    for(size_t p = 0; p <= clen; p++) {
        if(p != clen && content[p] != '\n') continue;

        const char *raw = content + line_start;
        size_t raw_len = p - line_start;
        line_start = p + 1;

        if(is_blank_or_divider(raw, raw_len)) {
            field = FIELD_NONE;
            continue;
        }

        const char *text;
        size_t text_len;
        trim(raw, raw_len, &text, &text_len);
        if(text_len > 0 && text[0] == '*') trim(text + 1, text_len - 1, &text, &text_len);
        if(text_len == 0) {
            field = FIELD_NONE;
            continue;
        }

        const char *value_start;
        size_t value_len;
        size_t rest_off;

        if(match_colon_label(text, text_len, "Method", &value_start, &value_len) || match_colon_label(text, text_len, "Routine", &value_start, &value_len)) {
            buffer_join(&name_buf, " ", value_start, value_len);
            field = FIELD_NAME;
            found_marker = 1;
        } else if(match_colon_label(text, text_len, "Function", &value_start, &value_len) ||match_colon_label(text, text_len, "Logic", &value_start, &value_len)) {
            buffer_join(&brief_buf, " ", value_start, value_len);
            field = FIELD_BRIEF;
            found_marker = 1;
        } else if(match_tag(text, text_len, "@param", &rest_off)) {
            const char *rest;
            size_t rest_len;
            trim(text + rest_off, text_len - rest_off, &rest, &rest_len);
            size_t name_len = 0;
            while(name_len < rest_len && is_ident(rest[name_len])) name_len++;
            const char *desc_start;
            size_t desc_len;
            trim(rest + name_len, rest_len - name_len, &desc_start, &desc_len);

            if(param_count == param_cap) {
                param_cap = param_cap ? param_cap * 2 : 4;
                params = xrealloc(params, param_cap * sizeof(InputParam));
                param_descs = xrealloc(param_descs, param_cap * sizeof(Buffer));
            }
            params[param_count].name = xstrndup(rest, name_len);
            params[param_count].description = NULL;
            param_descs[param_count] = (Buffer){0};
            if(desc_len > 0) buffer_put(&param_descs[param_count], (char *)desc_start, desc_len);
            cur_param = param_count;
            param_count++;
            field = FIELD_PARAM;
            found_marker = 1;
        } else if(match_tag(text, text_len, "@returns", &rest_off) || match_tag(text, text_len, "@return", &rest_off)) {
            const char *val;
            size_t val_len;
            trim(text + rest_off, text_len - rest_off, &val, &val_len);
            if(val_len > 0 && *val == ':') trim(val + 1, val_len - 1, &val, &val_len);
            buffer_join(&returns_buf, " ", val, val_len);
            field = FIELD_RETURNS;
        } else if(match_tag(text, text_len, "@throws", &rest_off) || match_tag(text, text_len, "@exception", &rest_off)) {
            const char *val;
            size_t val_len;
            trim(text + rest_off, text_len - rest_off, &val, &val_len);
            if(val_len > 0 && *val == ':') trim(val + 1, val_len - 1, &val, &val_len);
            buffer_join(&notes_buf, "; ", val, val_len);
            field = FIELD_NOTES;
        } else {
            switch(field) {
                case FIELD_NAME:    buffer_join(&name_buf, " ", text, text_len); break;
                case FIELD_BRIEF:   buffer_join(&brief_buf, " ", text, text_len); break;
                case FIELD_RETURNS: buffer_join(&returns_buf, " ", text, text_len); break;
                case FIELD_NOTES:   buffer_join(&notes_buf, " ", text, text_len); break;
                case FIELD_PARAM:   buffer_join(&param_descs[cur_param], " ", text, text_len); break;
                case FIELD_NONE:    break;
            }
        }
    }

    if(!found_marker) {
        free(name_buf.data);
        free(brief_buf.data);
        free(returns_buf.data);
        free(notes_buf.data);
        for(size_t k = 0; k < param_count; k++) {
            free(params[k].name);
            free(param_descs[k].data);
        }
        free(params);
        free(param_descs);
        return 0;
    }

    out->name = name_buf.len > 0 ? name_buf.data : NULL;
    out->signature = NULL;
    out->description = brief_buf.len > 0 ? brief_buf.data : NULL;
    out->output = returns_buf.len > 0 ? returns_buf.data : NULL;
    out->diagram = NULL;
    out->notes = notes_buf.len > 0 ? notes_buf.data : NULL;
    out->type = NULL;  /* no Java symbol-kind logic yet */

    for(size_t k = 0; k < param_count; k++) {
        symbol_add_input(out, params[k].name,
                          param_descs[k].len > 0 ? param_descs[k].data : "");
        free(params[k].name);
        free(param_descs[k].data);
    }
    free(params);
    free(param_descs);
    symbol_shrink_inputs_to_fit(out);
    return 1;
}
