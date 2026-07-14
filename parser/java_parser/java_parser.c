#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "java_parser.h"
#include "util.h"
#include "doc_comment.h"
#include "../shared/file_buffer.h"

/**
 * @brief Skip whitespace, comments, and annotations after a doc comment,
 *        up to the declaration they precede.
 *
 * Handles stacked annotations (e.g. @Override followed by
 * @SuppressWarnings("unchecked")) and annotations with nested-paren
 * arguments, plus any line or block comments interleaved between them.
 *
 * @param src Source buffer.
 * @param start Offset to start scanning from.
 * @param len Length of src in bytes.
 * @return Index of the first non-whitespace, non-comment, non-annotation
 *         character at or after start.
 */
static size_t skip_annotations(const char *src, size_t start, size_t len) {
    size_t i = start;
    for(;;) {
        size_t after_ws = skip_whitespace(src, i, len);
        if(after_ws + 1 < len && src[after_ws] == '/' && src[after_ws + 1] == '/') {
            after_ws = skip_line_comment(src, after_ws, len);
            i = after_ws; 
            continue;
        }

        if(after_ws + 1 < len && src[after_ws] == '/' && src[after_ws + 1] == '*'){
            size_t end = after_ws + 2;
            while(end < len && !(src[end] == '*' && end + 1 < len && src[end + 1] == '/')) end++;
            i = end < len ? end + 2 : len;
            continue;
        }
        if(after_ws >= len || src[after_ws] != '@') return after_ws;

        size_t j = after_ws + 1;
        while(j < len && (isalnum((unsigned char)src[j]) || src[j] == '_' || src[j] == '.')) j++;

        size_t after_name = skip_whitespace(src, j, len);
        if(after_name < len && src[after_name] =='(') {
            int depth = 0;
            j = after_name;
            do {
                if(src[j] == '(') depth++;
                else if(src[j] ==')')depth--;
                j++;
            } while(j < len && depth > 0);
        }

        i = j;
    }
}

/**
 * @brief Extract a declaration's signature text following a doc comment.
 *
 * Copies src[start..) up to (not including) the first '{' or ';',
 * collapsing every run of whitespace to a single space.
 *
 * @param src Source buffer.
 * @param start Offset to start extracting from.
 * @param len Length of src in bytes.
 * @return Newly heap-allocated, whitespace-collapsed signature string, or
 *         NULL if there was nothing there.
 */
static char *extract_signature(const char *src, size_t start, size_t len) {
    Buffer b = {0};
    int pending_space = 0;

    size_t i = start;
    while(i < len && src[i] != '{' && src[i] != ';') {
        if(isspace((unsigned char)src[i])) {
            if(b.len > 0) pending_space = 1;
        } else {
            if(pending_space) buffer_put(&b, " ", 1);
            pending_space = 0;
            buffer_put(&b, (char *)(src + i), 1);  // cast needed until buffer_put takes const char *
        }
        i++;
    }

    if(b.len == 0) return NULL;
    return b.data;
}

/**
 * @brief Compute the 1-based line number of a byte offset into src.
 *
 * *anchor_pos and *anchor_line carry the position/line of the previous
 * call so each call only scans the gap since then, instead of re-scanning
 * from the start of the file every time. Callers must only call this with
 * non-decreasing pos values (true for java_parse's single forward pass);
 * if pos precedes the anchor, it falls back to a full re-scan from the
 * start.
 *
 * @param src Source buffer.
 * @param pos Byte offset to find the line number of.
 * @param anchor_pos Position of the previous call, updated to pos.
 * @param anchor_line Line number of the previous call, updated to the
 *                     result.
 * @return 1-based line number of pos.
 */
static uint32_t line_of(const char *src, size_t pos, size_t *anchor_pos, uint32_t *anchor_line) {
    size_t k = pos >= *anchor_pos ? *anchor_pos : 0;
    uint32_t line = pos >= *anchor_pos ? *anchor_line : 1;
    for(; k < pos; k++)
        if(src[k] == '\n') line++;
    *anchor_pos = pos;
    *anchor_line = line;
    return line;
}

/**
 * @brief Check whether a token is a Java method/constructor modifier keyword.
 *
 * Modifier keywords are the things that can precede a return type (or a
 * constructor name): public, private, protected, static, final, abstract,
 * synchronized, native, strictfp, default.
 *
 * @param src Start of the token (not necessarily NUL-terminated there).
 * @param len Length of the token in bytes.
 * @return 1 if the token exactly matches one of the modifier keywords, 0
 *         otherwise.
 */
static int is_modifier(const char *src, size_t len) {
    static const char *mods[] = {
        "public", "private", "protected", "static", "final", "abstract",
        "synchronized", "native", "strictfp", "default"
    };
    for(size_t i = 0; i < sizeof(mods) / sizeof(mods[0]); i++)
        if(strlen(mods[i]) == len && strncmp(src, mods[i], len) == 0) return 1;
    return 0;
}

/**
 * @brief Classify a symbol as "constructor" or "method" from its signature.
 *
 * A Java constructor has no return type: once modifiers and any generic
 * <...> clause are stripped from the text before the name, a constructor
 * has nothing left while a method still carries its return type.
 *
 * @param sig Extracted signature text (as produced by extract_signature).
 * @param name Symbol name, expected to appear as the trailing token of
 *             sig's prefix before '('.
 * @return Newly heap-allocated "method" or "constructor" string, or NULL
 *         when it cannot be determined (no signature / name).
 */
static char *classify_type(const char *sig, const char *name) {
    if(!sig || !name) return NULL;
    const char *paren = strchr(sig, '(');
    if(!paren) return NULL;

    // Prefix = everything before '(', minus the trailing name token.
    size_t prefix_len = (size_t)(paren - sig);
    while(prefix_len && isspace((unsigned char)sig[prefix_len - 1])) prefix_len--;
    size_t name_len = strlen(name);
    if(prefix_len >= name_len &&
       strncmp(sig + prefix_len - name_len, name, name_len) == 0)
        prefix_len -= name_len;

    // Any non-modifier, non-generic token left in the prefix is a return type.
    int has_return_type = 0;
    for(size_t i = 0; i < prefix_len; ) {
        while(i < prefix_len && isspace((unsigned char)sig[i])) i++;
        size_t start = i;
        while(i < prefix_len && !isspace((unsigned char)sig[i])) i++;
        size_t tlen = i - start;
        if(tlen == 0) break;
        if(sig[start] == '<') continue;            // generic type parameter list
        if(is_modifier(sig + start, tlen)) continue;
        has_return_type = 1;
    }
    const char *kind = has_return_type ? "method" : "constructor";
    return xstrndup(kind, strlen(kind));
}

/**
 * @brief Extract the method or constructor name from a signature.
 *
 * The name is the identifier immediately preceding the first '(' in sig.
 *
 * @param sig Extracted signature text (as produced by extract_signature).
 * @return Newly heap-allocated name string, or NULL if sig is NULL or no
 *         identifier immediately precedes '('.
 */
static char *extract_name(const char *sig) {
    if(!sig) return NULL;
    const char *p = strchr(sig, '(');
    if(!p) return NULL;
    while(p > sig && isspace((unsigned char)p[-1])) p--;
    const char *end = p;
    while(p > sig && (isalnum((unsigned char)p[-1]) || p[-1] == '_')) p--;
    if(p == end) return NULL;
    return xstrndup(p, (size_t)(end - p));
}

/**
 * @brief Parse a Java source file into a Module of documented symbols.
 *
 * Scans the file for Javadoc-style doc comments (block comments that open
 * with an extra leading asterisk), skipping over string and character
 * literals, line comments, and plain block comments so their contents
 * can't be mistaken for one. Each doc comment is parsed via
 * parse_doc_comment(); for every one that yields a recognized Symbol, this
 * also captures the signature of the declaration that follows (skipping
 * over annotations via skip_annotations()), the method/constructor name,
 * its 1-based line number, and its classified type ("method" or
 * "constructor").
 *
 * @param path Path to the Java source file to parse.
 * @return Newly allocated Module with one Symbol per recognized doc
 *         comment, or NULL if the file could not be read (mirroring
 *         cp_parser's contract so parser_interface.c's caller sees this as
 *         a real failure rather than an empty-but-successfully-parsed
 *         module).
 */
Module *java_parse(const char *path) {
    Module *m = init_module(path);

    FileBuffer fb = read_file_buffer(path);
    if(!fb.data) {
        // read_file_buffer already reported the failure; mirror cp_parser's
        // contract (return NULL) so parser_interface.c's caller sees this as
        // a real failure instead of an empty-but-successfully-parsed module.
        free_module(m);
        return NULL;
    }
    const char *src = fb.data;
    size_t len = fb.len;

    size_t line_anchor_pos = 0;
    uint32_t line_anchor_line = 1;

    size_t i = 0;
    while(i < len) {
        // Skip over string literals, character literals, and comments to find doc comments
        if(src[i] == '"' || src[i] == '\'') {
            char delim = src[i++];
            while(i < len && src[i] != delim) {
                if(src[i] == '\\') i++;  // skip escaped character
                i++;
            }
            if(i < len) i++;  // skip closing delimiter
            continue;
        }

        if(src[i] == '/' && i + 1 < len && src[i + 1] == '/') {
            i = skip_line_comment(src, i, len);
            continue;
        }

        if(src[i] == '/' && i + 1 < len && src[i + 1] == '*') {
            // Find the closing */ regardless of comment type so we always advance past it
            size_t scan_start = i + 2;
            size_t end = scan_start;
            while(end < len && !(src[end] == '*' && end + 1 < len && src[end + 1] == '/')) end++;
            size_t content_end = end < len ? end : len;
            size_t next = end < len ? end + 2 : len;

            // If it's a doc comment, parse it and extract the signature and name
            if(scan_start < len && src[scan_start] == '*') {
                size_t content_start = scan_start + 1;  // start after /**
                size_t content_len = content_end > content_start ? content_end - content_start : 0;
                Symbol sym = {0};
                if(parse_doc_comment(src + content_start, content_len, &sym)) {
                    size_t sig_start = skip_annotations(src, next, len);  // skip annotations so they aren't mistaken for the signature
                    sym.signature   = extract_signature(src, sig_start, len);
                    free(sym.name);
                    sym.name        = extract_name(sym.signature);  // Extract the method or constructor name from the signature
                    sym.line        = line_of(src, sig_start, &line_anchor_pos, &line_anchor_line);
                    sym.type        = classify_type(sym.signature, sym.name);  // "method" or "constructor"
                    *module_add_symbol(m) = sym;
                } else {
                    free_symbol_content(&sym);  //  Free any partially filled symbol if parsing failed
                }
            }

            i = next;
            continue;
        }

        size_t after_ws = skip_whitespace(src, i, len);
        i = after_ws > i ? after_ws : i + 1;
    }

    free_file_buffer(&fb);
    return m;
}