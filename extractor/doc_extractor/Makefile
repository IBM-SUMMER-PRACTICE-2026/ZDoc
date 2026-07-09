CC     ?= cc
CFLAGS ?= -O2 -Wall -Wextra -std=c11 -I.

# doc_extractor is a library now, not a standalone CLI - it converts an
# already-walked module tree plus an already-parsed array of modules into
# the combined DxModel (see doc_extractor.h) and hands that C struct
# directly to whichever renderer needs it, in the same process. No walking,
# no parsing, no linking against any parser source, no JSON/serialization
# step of any kind. Its only consumer is expected to be the daemon
# (zdoc/zdoc_deamon.c, on 16-feature-global-parser-header-file), which is
# not built or touched from here.
#
# thread_pool.o, bin_group.o, child_parser.o, parser_link.o, path_build.o,
# json_read.o, and dx_write.o (the JSON writer - no longer needed now that
# both renderers take the DxModel directly) are all disabled - quarantined
# in src/_disabled/ (gitignored). See doc_extractor.c's top-of-file note.
LIB_OBJS = src/xalloc.o src/doc_extractor.o \
           module_tree/modtree_table.o

TEST_BIN  = dx_build_test
TEST_OBJS = tests/dx_build_test.o

LDLIBS ?=

all: $(LIB_OBJS)

src/xalloc.o: src/xalloc.c src/xalloc.h
src/doc_extractor.o: src/doc_extractor.c src/doc_extractor.h src/xalloc.h module_tree/modtree_tables.h

module_tree/modtree_table.o: module_tree/modtree_table.c module_tree/modtree_tables.h

tests/dx_build_test.o: tests/dx_build_test.c src/doc_extractor.h module_tree/modtree_tables.h

$(TEST_BIN): $(LIB_OBJS) $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $(LIB_OBJS) $(TEST_OBJS) $(LDLIBS)

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -f $(LIB_OBJS) $(TEST_OBJS) $(TEST_BIN)

.PHONY: all test clean
