/*
 * Unit tests for plx_parser, exercised against two fixtures:
 *   - tests/basic_doc.plx: one documented procedure, one undocumented one
 *   - tests/no_doc_comments.plx: regression fixture, must yield zero symbols
 * Asserts on exact expected symbols instead of relying on a human to
 * eyeball printed output.
 */
#include "../plx_parser.h"

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

static void test_basic_doc(void) {
    Module *m = plx_parse_file("tests/basic_doc.plx");
    CHECK(m != NULL, "basic_doc: plx_parse_file returned NULL");
    if (!m) return;

    CHECK(m->symbolCount == 1, "basic_doc: expected exactly 1 documented procedure");

    for (int i = 0; i < m->symbolCount; i++)
        CHECK(strcmp(m->symbols[i].name, "UNDOCUMENTED_PROC") != 0,
              "basic_doc: procedure with no doc-comment block must be skipped");

    if (m->symbolCount > 0) {
        Symbol *s = &m->symbols[0];
        CHECK_STR(s->name, "ADD_NUMBERS", "basic_doc: symbol[0] name");
        CHECK_STR(s->type, "procedure", "basic_doc: symbol[0] type");
        CHECK(s->line == 14, "basic_doc: symbol[0] line");
        CHECK_STR(s->description, "Adds two integers and returns the sum",
                  "basic_doc: symbol[0] description");
        CHECK_STR(s->output, "Sum of A and B", "basic_doc: symbol[0] output");
        CHECK(s->inputCount == 2, "basic_doc: symbol[0] param count");
        if (s->inputCount == 2) {
            CHECK_STR(s->input[0].name, "A", "basic_doc: symbol[0] param[0] name");
            CHECK_STR(s->input[0].description, "first operand",
                      "basic_doc: symbol[0] param[0] desc");
            CHECK_STR(s->input[1].name, "B", "basic_doc: symbol[0] param[1] name");
            CHECK_STR(s->input[1].description, "second operand",
                      "basic_doc: symbol[0] param[1] desc");
        }
    }

    plx_free_module(m);
}

static void test_no_doc_comments(void) {
    Module *m = plx_parse_file("tests/no_doc_comments.plx");
    CHECK(m != NULL, "no_doc_comments: plx_parse_file returned NULL");
    if (!m) return;

    CHECK(m->symbolCount == 0,
          "no_doc_comments: file with only plain code comments must yield zero symbols");

    plx_free_module(m);
}

int main(void) {
    test_basic_doc();
    test_no_doc_comments();

    if (failures) {
        fprintf(stderr, "plx_parser_test: %d assertion(s) FAILED\n", failures);
        return 1;
    }
    printf("plx_parser_test: all assertions passed\n");
    return 0;
}
