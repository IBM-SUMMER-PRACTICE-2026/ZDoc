CC     ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -g

# html_renderer takes doc_extractor's DxModel directly - no JSON, no
# subprocess, no separate wire format. It's a library, not a standalone CLI
# (see _disabled/main.c for the retired JSON-reading CLI driver). Its own
# sources need doc_extractor.h's module_tree include to resolve, so every
# compile here gets -I$(DX_DIR).
DX_DIR = ../../extractor/doc_extractor
CFLAGS += -I$(DX_DIR)

LIB_OBJS = html_render.o \
           dx_doc_extractor.o dx_xalloc.o dx_modtree_table.o

TEST_BIN  = render_test
TEST_OBJS = tests/render_test.o

all: $(LIB_OBJS)

html_render.o: html_render.c html_renderer.h $(DX_DIR)/src/doc_extractor.h

# doc_extractor's own sources, compiled straight into this binary so
# DxModel can be built and rendered in one process, no JSON in between.
dx_doc_extractor.o: $(DX_DIR)/src/doc_extractor.c $(DX_DIR)/src/doc_extractor.h $(DX_DIR)/src/xalloc.h
	$(CC) $(CFLAGS) -c $(DX_DIR)/src/doc_extractor.c -o $@
dx_xalloc.o: $(DX_DIR)/src/xalloc.c $(DX_DIR)/src/xalloc.h
	$(CC) $(CFLAGS) -c $(DX_DIR)/src/xalloc.c -o $@
dx_modtree_table.o: $(DX_DIR)/module_tree/modtree_table.c $(DX_DIR)/module_tree/modtree_tables.h
	$(CC) $(CFLAGS) -c $(DX_DIR)/module_tree/modtree_table.c -o $@

tests/render_test.o: tests/render_test.c html_renderer.h $(DX_DIR)/src/doc_extractor.h

$(TEST_BIN): $(LIB_OBJS) $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $(LIB_OBJS) $(TEST_OBJS)

test: $(TEST_BIN)
	rm -rf tests/tmp
	./$(TEST_BIN) tests/tmp
	sh tests/check.sh

clean:
	rm -f $(LIB_OBJS) $(TEST_OBJS) $(TEST_BIN)
	rm -rf tests/tmp

.PHONY: all test clean
