/*
 * Unit tests for parser_shared_interface.c - the growth-tracked Module/
 * Symbol/InputParam allocator all three parsers now share. The three
 * per-parser test suites (c_parser_test.c, java_parser_test.c,
 * plx_parser_test.c) exercise this code indirectly, but none of their
 * fixtures are big enough to cross a capacity-doubling boundary or pass a
 * path with a directory component - both of those are covered here
 * instead.
 */
#include "../parser_shared.h"

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

/* symbol_add_input starts at cap 8 and doubles - 20 inputs forces two
 * growth rounds (8 -> 16 -> 32) and checks every entry survives intact. */
static void test_symbol_add_input_growth(void) {
    Symbol sym;
    memset(&sym, 0, sizeof sym);

    char name[16], desc[32];
    for (int i = 0; i < 20; i++) {
        snprintf(name, sizeof name, "p%d", i);
        snprintf(desc, sizeof desc, "description %d", i);
        symbol_add_input(&sym, name, desc);
    }

    CHECK(sym.inputCount == 20, "20 inputs added, inputCount should be 20");
    CHECK(sym.inputCap >= 20, "inputCap should have grown to at least 20");

    for (int i = 0; i < 20; i++) {
        snprintf(name, sizeof name, "p%d", i);
        snprintf(desc, sizeof desc, "description %d", i);
        CHECK_STR(sym.input[i].name, name, "input name preserved after growth");
        CHECK_STR(sym.input[i].description, desc, "input description preserved after growth");
    }

    symbol_shrink_inputs_to_fit(&sym);
    CHECK(sym.inputCap == sym.inputCount, "shrink_inputs_to_fit: cap must equal count");
    CHECK(sym.inputCount == 20, "shrink_inputs_to_fit must not change count");
    CHECK_STR(sym.input[19].name, "p19", "last input still correct after shrink");

    free_symbol_content(&sym);
}

/* Shrinking down to zero inputs must free the array and null the pointer,
 * not just report cap == count == 0. */
static void test_symbol_shrink_to_zero(void) {
    Symbol sym;
    memset(&sym, 0, sizeof sym);

    symbol_shrink_inputs_to_fit(&sym);
    CHECK(sym.input == NULL, "shrinking a symbol with zero inputs must leave input NULL");
    CHECK(sym.inputCap == 0, "shrinking a symbol with zero inputs must leave inputCap 0");

    free_symbol_content(&sym);
}

/* module_add_symbol starts at cap 8 and doubles - 20 symbols forces the
 * same two growth rounds at the Module level. */
static void test_module_add_symbol_growth(void) {
    Module *mod = init_module("some/dir/module.plx");

    for (int i = 0; i < 20; i++) {
        Symbol *s = module_add_symbol(mod);
        CHECK(s != NULL, "module_add_symbol must return a usable slot");
        char name[16];
        snprintf(name, sizeof name, "sym%d", i);
        s->name = xstrdup(name);
    }

    CHECK(mod->symbolCount == 20, "20 symbols added, symbolCount should be 20");
    CHECK(mod->symbolCap >= 20, "symbolCap should have grown to at least 20");

    for (int i = 0; i < 20; i++) {
        char name[16];
        snprintf(name, sizeof name, "sym%d", i);
        CHECK_STR(mod->symbols[i].name, name, "symbol name preserved after growth");
    }

    module_shrink_to_fit(mod);
    CHECK(mod->symbolCap == mod->symbolCount, "module_shrink_to_fit: cap must equal count");
    CHECK_STR(mod->symbols[19].name, "sym19", "last symbol still correct after shrink");

    free_module(mod);
}

/* Shrinking a module with zero symbols must free the array, not just
 * report cap == count == 0 - mirrors the Symbol-level zero case above. */
static void test_module_shrink_to_zero(void) {
    Module *mod = init_module("empty.c");

    module_shrink_to_fit(mod);
    CHECK(mod->symbols == NULL, "shrinking a module with zero symbols must leave symbols NULL");
    CHECK(mod->symbolCap == 0, "shrinking a module with zero symbols must leave symbolCap 0");

    free_module(mod);
}

/* init_module must store only the basename, not the full path handed in -
 * none of the three parser test suites ever pass a path with a directory
 * component, so this was never actually checked anywhere. */
static void test_init_module_basename(void) {
    Module *mod;

    mod = init_module("src/a/Foo.java");
    CHECK_STR(mod->filename, "Foo.java", "nested unix path: basename only");
    free_module(mod);

    mod = init_module("Bare.c");
    CHECK_STR(mod->filename, "Bare.c", "bare filename: unchanged");
    free_module(mod);

    mod = init_module("dir/subdir/");
    CHECK_STR(mod->filename, "", "path ending in a slash: empty basename, not the last dir name");
    free_module(mod);

    mod = init_module("win\\style\\path.plx");
    CHECK_STR(mod->filename, "path.plx", "backslash-separated path: basename only");
    free_module(mod);
}

int main(void) {
    test_symbol_add_input_growth();
    test_symbol_shrink_to_zero();
    test_module_add_symbol_growth();
    test_module_shrink_to_zero();
    test_init_module_basename();

    if (failures) {
        fprintf(stderr, "parser_shared_interface_test: %d assertion(s) FAILED\n", failures);
        return 1;
    }
    printf("parser_shared_interface_test: all assertions passed\n");
    return 0;
}
