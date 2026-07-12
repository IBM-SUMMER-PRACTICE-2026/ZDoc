CC     ?= cc
CFLAGS ?= -O2 -Wall -Wextra -std=c11 -I.

# md_renderer takes doc_extractor's DxModel directly now - no JSON, no
# subprocess, no separate wire format. It's a library, not a standalone CLI
# (see src/_disabled/main.c for the retired JSON-reading CLI driver). Its
# own sources need doc_extractor.h's module_tree include to resolve, so
# every compile here gets -I$(DX_DIR).
DX_DIR = ../../extractor/doc_extractor
CFLAGS += -I$(DX_DIR)

LIB_OBJS = src/md_path.o src/md_render.o \
           src/dx_doc_extractor.o src/dx_xalloc.o src/dx_modtree_table.o

TEST_BIN  = render_test
TEST_OBJS = tests/render_test.o

all: $(LIB_OBJS)

src/md_path.o: src/md_path.c src/md_renderer.h $(DX_DIR)/src/doc_extractor.h
src/md_render.o: src/md_render.c src/md_renderer.h $(DX_DIR)/src/doc_extractor.h

# doc_extractor's own sources, compiled straight into this binary so
# DxModel can be built and rendered in one process, no JSON in between.
src/dx_doc_extractor.o: $(DX_DIR)/src/doc_extractor.c $(DX_DIR)/src/doc_extractor.h $(DX_DIR)/src/xalloc.h
	$(CC) $(CFLAGS) -c $(DX_DIR)/src/doc_extractor.c -o $@
src/dx_xalloc.o: $(DX_DIR)/src/xalloc.c $(DX_DIR)/src/xalloc.h
	$(CC) $(CFLAGS) -c $(DX_DIR)/src/xalloc.c -o $@
src/dx_modtree_table.o: $(DX_DIR)/module_tree/modtree_table.c $(DX_DIR)/module_tree/modtree_tables.h
	$(CC) $(CFLAGS) -c $(DX_DIR)/module_tree/modtree_table.c -o $@

tests/render_test.o: tests/render_test.c src/md_renderer.h $(DX_DIR)/src/doc_extractor.h

$(TEST_BIN): $(LIB_OBJS) $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $(LIB_OBJS) $(TEST_OBJS)

test: $(TEST_BIN)
	rm -rf zdoc-out-test
	./$(TEST_BIN) ./zdoc-out-test

clean:
	rm -f $(LIB_OBJS) $(TEST_OBJS) $(TEST_BIN)
	rm -rf zdoc-out-test

.PHONY: all test clean
