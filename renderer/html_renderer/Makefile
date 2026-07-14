CC     ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -g

# html_renderer takes the daemon's already-walked module_tree tables and
# parsed Module array directly - no JSON, no subprocess, no intermediate
# doc_extractor model in between. It's a library, not a standalone CLI (see
# _disabled/main.c for the retired JSON-reading CLI driver). Its own
# sources need modtree_tables.h to resolve, so every compile here gets
# -I$(MODTREE_DIR).
MODTREE_DIR = ../../extractor/doc_extractor
CFLAGS += -I$(MODTREE_DIR)

LIB_OBJS = html_render.o dx_modtree_table.o

TEST_BIN  = render_test
TEST_OBJS = tests/render_test.o

all: $(LIB_OBJS)

html_render.o: html_render.c html_renderer.h $(MODTREE_DIR)/module_tree/modtree_tables.h

# module_tree's own source, compiled straight into this binary for
# modtree_file_path/modtree_dir_path (path reconstruction).
dx_modtree_table.o: $(MODTREE_DIR)/module_tree/modtree_table.c $(MODTREE_DIR)/module_tree/modtree_tables.h
	$(CC) $(CFLAGS) -c $(MODTREE_DIR)/module_tree/modtree_table.c -o $@

tests/render_test.o: tests/render_test.c html_renderer.h $(MODTREE_DIR)/module_tree/modtree_tables.h

$(TEST_BIN): $(LIB_OBJS) $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $(LIB_OBJS) $(TEST_OBJS)

test: $(TEST_BIN)
	rm -rf tests/tmp
	./$(TEST_BIN)
	rm -rf tests/tmp

clean:
	rm -f $(LIB_OBJS) $(TEST_OBJS) $(TEST_BIN)
	rm -rf tests/tmp

.PHONY: all test clean
