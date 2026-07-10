/*
 Unit tests for doc_extractor.dx_build-from_parsed dx_build_from_parsed converting an
 already walked module tree plus an already parsed Module array into a
 DxModel. 
 */
#include "../src/doc_extractor.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static InputParam make_param(const char *name, const char *desc) {
    InputParam p;
    p.name = (char *)name;
    p.description = (char *)desc;
    return p;
}

/* A file whose module never parsed (module_count == 0): language is still
 derived from the extension, but the file is left in its pessimistic
 error == 1 default with no symbols. */
static void test_no_parsed_modules(void) {
    modtree_dir_table_t dirs;
    modtree_file_table_t files;
    modtree_dir_table_init(&dirs);
    modtree_file_table_init(&files);

    int root = modtree_intern_dir(&dirs, "proj", -1);
    modtree_intern_file(&files, "main.c", root);

    DxModel m;
    int ok = dx_build_from_parsed(&dirs, &files, NULL, 0, &m);
    assert(ok == 1);
    assert(m.dir_count == 1);
    assert(m.file_count == 1);
    assert(strcmp(m.files[0].name, "main.c") == 0);
    assert(strcmp(m.files[0].language, "c") == 0);
    assert(m.files[0].error == 1);
    assert(m.files[0].symbol_count == 0);

    dx_free(&m);
    modtree_dir_table_free(&dirs);
    modtree_file_table_free(&files);
    printf("test_no_parsed_modules passed\n");
}

/* A file with an unrecognized extension: language stays NULL and the file
  is skipped even when a matching Module is present at its index - the
  if(!lang) continue; check runs before the module is ever looked at. */
static void test_unknown_extension_skips_module(void) {
    modtree_dir_table_t dirs;
    modtree_file_table_t files;
    modtree_dir_table_init(&dirs);
    modtree_file_table_init(&files);

    int root = modtree_intern_dir(&dirs, "proj", -1);
    modtree_intern_file(&files, "README", root);

    Symbol sym;
    memset(&sym, 0, sizeof sym);
    sym.name = (char *)"ignored";
    sym.type = (char *)"function";

    Module mod;
    memset(&mod, 0, sizeof mod);
    mod.filename    = (char *)"README";
    mod.pathIndex   = 0;
    mod.symbols     = &sym;
    mod.symbolCount = 1;

    DxModel m;
    int ok = dx_build_from_parsed(&dirs, &files, &mod, 1, &m);
    assert(ok == 1);
    assert(m.files[0].language == NULL);
    assert(m.files[0].error == 1);
    assert(m.files[0].symbol_count == 0);

    dx_free(&m);
    modtree_dir_table_free(&dirs);
    modtree_file_table_free(&files);
    printf("test_unknown_extension_skips_module passed\n");
}

/* A matched module converts every Symbol field 1:1 into a DxSymbol,
  including its params - refs/ref_count have no upstream source yet and
  must stay NULL/0. */
static void test_matched_symbol_conversion(void) {
    modtree_dir_table_t dirs;
    modtree_file_table_t files;
    modtree_dir_table_init(&dirs);
    modtree_file_table_init(&files);

    int root = modtree_intern_dir(&dirs, "proj", -1);
    modtree_intern_file(&files, "util.plx", root);

    InputParam params[2];
    params[0] = make_param("n", "count of items");
    params[1] = make_param("buf", "destination buffer");

    Symbol sym;
    memset(&sym, 0, sizeof sym);
    sym.name        = (char *)"do_thing";
    sym.description = (char *)"does the thing";
    sym.signature   = (char *)"void do_thing(int n, char *buf)";
    sym.input       = params;
    sym.inputCount  = 2;
    sym.output      = (char *)"void";
    sym.notes       = (char *)"call once at startup";
    sym.line        = 42;
    sym.type        = (char *)"procedure";
    sym.diagram     = (char *)"flowchart TD; A-->B";

    Module mod;
    memset(&mod, 0, sizeof mod);
    mod.filename    = (char *)"util.plx";
    mod.pathIndex   = 0;
    mod.symbols     = &sym;
    mod.symbolCount = 1;

    DxModel m;
    int ok = dx_build_from_parsed(&dirs, &files, &mod, 1, &m);
    assert(ok == 1);
    assert(m.files[0].error == 0);
    assert(strcmp(m.files[0].language, "plx") == 0);
    assert(m.files[0].symbol_count == 1);

    DxSymbol *ds = &m.files[0].symbols[0];
    assert(strcmp(ds->kind, "procedure") == 0);
    assert(ds->line == 42);
    assert(strcmp(ds->name, "do_thing") == 0);
    assert(strcmp(ds->signature, "void do_thing(int n, char *buf)") == 0);
    assert(strcmp(ds->brief, "does the thing") == 0);
    assert(strcmp(ds->returns, "void") == 0);
    assert(strcmp(ds->notes, "call once at startup") == 0);
    assert(strcmp(ds->diagram, "flowchart TD; A-->B") == 0);
    assert(ds->param_count == 2);
    assert(strcmp(ds->params[0].name, "n") == 0);
    assert(strcmp(ds->params[0].desc, "count of items") == 0);
    assert(strcmp(ds->params[1].name, "buf") == 0);
    assert(strcmp(ds->params[1].desc, "destination buffer") == 0);
    assert(ds->refs == NULL);
    assert(ds->ref_count == 0);

    dx_free(&m);
    modtree_dir_table_free(&dirs);
    modtree_file_table_free(&files);
    printf("test_matched_symbol_conversion passed\n");
}

/* pathIndex is the only thing that ties a Module to a file - a Module
  present at the right array slot but stamped with a different pathIndex
  (e.g. the daemon parsed files out of order) must not be matched. */
static void test_pathindex_mismatch_leaves_error(void) {
    modtree_dir_table_t dirs;
    modtree_file_table_t files;
    modtree_dir_table_init(&dirs);
    modtree_file_table_init(&files);

    int root = modtree_intern_dir(&dirs, "proj", -1);
    modtree_intern_file(&files, "a.c", root);
    modtree_intern_file(&files, "b.c", root);

    Module mods[2];
    memset(mods, 0, sizeof mods);
    mods[0].filename  = (char *)"a.c";
    mods[0].pathIndex = 1; // wrong - should be 0 
    mods[1].filename  = (char *)"b.c";
    mods[1].pathIndex = 0; // wrong - should be 1 

    DxModel m;
    int ok = dx_build_from_parsed(&dirs, &files, mods, 2, &m);
    assert(ok == 1);
    assert(m.files[0].error == 1);
    assert(m.files[1].error == 1);
    assert(m.files[0].symbol_count == 0);
    assert(m.files[1].symbol_count == 0);
    // language is still derived independently of the match 
    assert(strcmp(m.files[0].language, "c") == 0);
    assert(strcmp(m.files[1].language, "c") == 0);

    dx_free(&m);
    modtree_dir_table_free(&dirs);
    modtree_file_table_free(&files);
    printf("test_pathindex_mismatch_leaves_error passed\n");
}

/* Fewer parsed modules than files (trailing files that haven't been parsed
 yet, or failed) must stay error == 1 without reading past module_count. */
static void test_partial_module_array(void) {
    modtree_dir_table_t dirs;
    modtree_file_table_t files;
    modtree_dir_table_init(&dirs);
    modtree_file_table_init(&files);

    int root = modtree_intern_dir(&dirs, "proj", -1);
    modtree_intern_file(&files, "a.java", root);
    modtree_intern_file(&files, "b.java", root);

    Module mods[1];
    memset(mods, 0, sizeof mods);
    mods[0].filename  = (char *)"a.java";
    mods[0].pathIndex = 0;

    DxModel m;
    int ok = dx_build_from_parsed(&dirs, &files, mods, 1, &m);
    assert(ok == 1);
    assert(m.file_count == 2);
    assert(m.files[0].error == 0);
    assert(m.files[1].error == 1); /* index 1 >= module_count */
    assert(strcmp(m.files[1].language, "java") == 0);

    dx_free(&m);
    modtree_dir_table_free(&dirs);
    modtree_file_table_free(&files);
    printf("test_partial_module_array passed\n");
}

/* dirs are copied 1:1 (name + parent_index), independent of any file or
 * module - covers the directory side of DxModel that the symbol-focused
 * tests above don't touch. */
static void test_dir_table_copy(void) {
    modtree_dir_table_t dirs;
    modtree_file_table_t files;
    modtree_dir_table_init(&dirs);
    modtree_file_table_init(&files);

    int root = modtree_intern_dir(&dirs, "proj", -1);
    int sub  = modtree_intern_dir(&dirs, "sub", root);
    modtree_intern_file(&files, "x.cpp", sub);

    DxModel m;
    int ok = dx_build_from_parsed(&dirs, &files, NULL, 0, &m);
    assert(ok == 1);
    assert(m.dir_count == 2);
    assert(strcmp(m.dirs[0].name, "proj") == 0);
    assert(m.dirs[0].parent_index == -1);
    assert(strcmp(m.dirs[1].name, "sub") == 0);
    assert(m.dirs[1].parent_index == root);
    assert(m.files[0].parent_dir_index == sub);
    assert(strcmp(m.files[0].language, "cpp") == 0);

    dx_free(&m);
    modtree_dir_table_free(&dirs);
    modtree_file_table_free(&files);
    printf("test_dir_table_copy passed\n");
}

int main(void) {
    test_no_parsed_modules();
    test_unknown_extension_skips_module();
    test_matched_symbol_conversion();
    test_pathindex_mismatch_leaves_error();
    test_partial_module_array();
    test_dir_table_copy();

    printf("\nAll doc_extractor checks passed.\n");
    return 0;
}
