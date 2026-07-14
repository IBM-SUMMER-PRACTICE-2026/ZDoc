/*
 * Unit tests for c_parser, exercised against tests/sample.c. Asserts on
 * exact expected symbols instead of just dumping output for a human to
 * eyeball - a mismatch here is a real regression, not something to read
 * and judge by hand.
 */
#include "../c_parser.h"
#include "../../shared/parser_shared.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            failures++; \
        } \
    } while (0)

#define CHECK_STR(a, b, msg) \
    CHECK((a) != NULL && (b) != NULL && strcmp((a), (b)) == 0, msg)

static void check_no_symbol_named(const Module *m, const char *name, const char *msg) {
    for (int i = 0; i < m->symbolCount; i++)
        CHECK(strcmp(m->symbols[i].name, name) != 0, msg);
}

int main(void) {
    Module *m = cp_parse_file("tests/sample.c");
    CHECK(m != NULL, "cp_parse_file returned NULL");
    if (!m) {
        fprintf(stderr, "aborting: no module to check further\n");
        return 1;
    }

    CHECK(m->symbolCount == 9, "expected exactly 9 documented symbols");

    /* Explicit negative coverage: the fixture calls these out by name as
     * things that must NOT be attached/extracted. */
    check_no_symbol_named(m, "SAMPLE_H_GUARD", "undocumented object-like macro must be skipped");
    check_no_symbol_named(m, "internal_counter", "variable preceded only by a plain comment must be skipped");

    if (m->symbolCount > 0) {
        Symbol *s = &m->symbols[0];
        CHECK_STR(s->name, "MAX_RETRIES", "symbol[0] name");
        CHECK_STR(s->type, "macro", "symbol[0] type");
        CHECK(s->line == 9, "symbol[0] line");
        CHECK_STR(s->description, "Maximum number of retries before giving up.", "symbol[0] brief");
    }

    if (m->symbolCount > 2) {
        Symbol *s = &m->symbols[2];
        CHECK_STR(s->name, "widget_init", "symbol[2] name");
        CHECK_STR(s->type, "prototype", "symbol[2] type");
        CHECK(s->line == 26, "symbol[2] line");
        CHECK_STR(s->description, "Initialise the widget subsystem.", "symbol[2] brief");
        CHECK_STR(s->output, "0 on success, 8 on storage failure", "symbol[2] returns");
        CHECK_STR(s->notes, "Allocates the anchor control block and chains it. Not thread safe.",
                  "symbol[2] notes");
        CHECK(s->inputCount == 2, "symbol[2] param count");
        if (s->inputCount == 2) {
            CHECK_STR(s->input[0].name, "anchor", "symbol[2] param[0] name");
            CHECK_STR(s->input[0].description, "pointer to the anchor block", "symbol[2] param[0] desc");
            CHECK_STR(s->input[1].name, "flags", "symbol[2] param[1] name");
            CHECK_STR(s->input[1].description, "initialisation flags", "symbol[2] param[1] desc");
        }
    }

    if (m->symbolCount > 8) {
        Symbol *s = &m->symbols[8];
        CHECK_STR(s->name, "widget_reset", "symbol[8] name");
        CHECK_STR(s->type, "function", "symbol[8] type");
        CHECK(s->line == 62, "symbol[8] line");
        CHECK_STR(s->description, "Line-comment style doc. Second line of the brief.",
                  "symbol[8] brief (multi-line // doc joined)");
    }

    cp_free_module(m);

    if (failures) {
        fprintf(stderr, "c_parser_test: %d assertion(s) FAILED\n", failures);
        return 1;
    }
    printf("c_parser_test: all assertions passed\n");
    return 0;
}
