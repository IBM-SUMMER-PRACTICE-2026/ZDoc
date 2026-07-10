/*
 * Unit tests for java_parser, exercised against tests/TestKek.java. Asserts
 * on exact expected symbols instead of relying on a human to eyeball
 * printed output.
 */
#include "../java_parser.h"

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
    Module *m = java_parse("tests/TestKek.java");
    CHECK(m != NULL, "java_parse returned NULL");
    if (!m) {
        fprintf(stderr, "aborting: no module to check further\n");
        return 1;
    }

    CHECK(m->symbolCount == 2, "expected exactly 2 documented methods/constructors");

    /* Explicit negative coverage: the fixture calls this out by name as a
     * method that must NOT be extracted (no doc comment above it). */
    check_no_symbol_named(m, "doStuff", "method with no doc comment must be skipped");

    if (m->symbolCount > 0) {
        Symbol *s = &m->symbols[0];
        CHECK_STR(s->name, "Kek", "symbol[0] name");
        CHECK_STR(s->type, "constructor", "symbol[0] type");
        CHECK(s->line == 11, "symbol[0] line");
        CHECK_STR(s->description,
                  "Creates a Kek object and loads the continuous data for the object",
                  "symbol[0] brief");
        CHECK(s->inputCount == 2, "symbol[0] param count");
        if (s->inputCount == 2) {
            CHECK_STR(s->input[0].name, "sadkek", "symbol[0] param[0] name");
            CHECK_STR(s->input[1].name, "kekw", "symbol[0] param[1] name");
        }
    }

    if (m->symbolCount > 1) {
        Symbol *s = &m->symbols[1];
        CHECK_STR(s->name, "loadKekData", "symbol[1] name");
        CHECK_STR(s->type, "method", "symbol[1] type");
        CHECK(s->line == 33, "symbol[1] line");
        CHECK_STR(s->output, "true if the data was loaded successfully, false otherwise",
                  "symbol[1] returns");
        CHECK(s->inputCount == 1, "symbol[1] param count");
        if (s->inputCount == 1)
            CHECK_STR(s->input[0].name, "path", "symbol[1] param[0] name");
        /* Two @throws lines must both be preserved, joined together. */
        CHECK(s->notes != NULL && strstr(s->notes, "IllegalArgumentException") != NULL,
              "symbol[1] notes contains first @throws");
        CHECK(s->notes != NULL && strstr(s->notes, "java.io.IOException") != NULL,
              "symbol[1] notes contains second @throws");
    }

    module_free(m);

    if (failures) {
        fprintf(stderr, "java_parser_test: %d assertion(s) FAILED\n", failures);
        return 1;
    }
    printf("java_parser_test: all assertions passed\n");
    return 0;
}
