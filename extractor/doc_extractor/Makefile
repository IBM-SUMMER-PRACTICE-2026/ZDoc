CC     ?= cc
CFLAGS ?= -O2 -Wall -Wextra -std=c11 -I.

BIN     = zdoc-doc-extractor
OBJS    = src/json_read.o src/child_parser.o src/doc_extractor.o src/dx_write.o src/main.o \
          src/thread_pool.o src/bin_group.o src/path_build.o src/plx_link.o \
          src/plx_parser.o src/plx_helpers.o \
          module_tree/fs_walk.o module_tree/modtree_table.o

# PL/X is linked in directly (parser/plx_parser/*.c compiled straight into
# this binary - see src/plx_link.c) instead of run as a subprocess. Its
# xmalloc/xrealloc/xstrdup/xstrndup helpers happen to share names with
# src/json_read.c's own - renaming them at compile time (via -D, not editing
# parser/plx_parser's source) avoids a duplicate-symbol link error. Nothing
# else in this binary calls PL/X's renamed helpers, only its public
# Module/Symbol/plx_parse_file/plx_free_module API.
PLX_DIR     = ../../parser/plx_parser
PLX_RENAMES = -Dxmalloc=plx_xmalloc -Dxrealloc=plx_xrealloc -Dxstrdup=plx_xstrdup -Dxstrndup=plx_xstrndup

# The thread pool uses pthreads on POSIX and native Win32 threads on Windows -
# only the former needs an explicit -lpthread (Win32 threading lives in
# kernel32, linked by default). $(OS) is set to "Windows_NT" by both cmd.exe
# and MinGW/MSYS shells, so this works whether make itself runs native or not.
ifeq ($(OS),Windows_NT)
LDLIBS ?=
else
LDLIBS ?= -lpthread
endif

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDLIBS)

src/json_read.o: src/json_read.c src/json_read.h
src/child_parser.o: src/child_parser.c src/child_parser.h src/doc_extractor.h src/json_read.h src/plx_link.h
src/doc_extractor.o: src/doc_extractor.c src/doc_extractor.h src/child_parser.h src/json_read.h \
                      src/bin_group.h src/path_build.h src/thread_pool.h \
                      module_tree/fs_walk.h module_tree/modtree_tables.h
src/thread_pool.o: src/thread_pool.c src/thread_pool.h src/child_parser.h src/doc_extractor.h src/json_read.h
src/bin_group.o: src/bin_group.c src/bin_group.h src/child_parser.h src/doc_extractor.h src/thread_pool.h src/json_read.h
src/path_build.o: src/path_build.c src/path_build.h module_tree/modtree_tables.h
src/dx_write.o: src/dx_write.c src/doc_extractor.h
src/main.o: src/main.c src/doc_extractor.h

src/plx_link.o: src/plx_link.c src/plx_link.h src/doc_extractor.h src/json_read.h \
                $(PLX_DIR)/helpers.h $(PLX_DIR)/plx_parser.h
src/plx_parser.o: $(PLX_DIR)/plx_parser.c $(PLX_DIR)/plx_parser.h $(PLX_DIR)/helpers.h
	$(CC) $(CFLAGS) $(PLX_RENAMES) -I$(PLX_DIR) -c $(PLX_DIR)/plx_parser.c -o $@
src/plx_helpers.o: $(PLX_DIR)/helpers.c $(PLX_DIR)/helpers.h
	$(CC) $(CFLAGS) $(PLX_RENAMES) -c $(PLX_DIR)/helpers.c -o $@

module_tree/fs_walk.o: module_tree/fs_walk.c module_tree/fs_walk.h module_tree/modtree_tables.h
module_tree/modtree_table.o: module_tree/modtree_table.c module_tree/modtree_tables.h

test: $(BIN)
	rm -rf out.json
	./$(BIN) --parser-dir tests/fake_parsers tests/fixture_project > out.json
	diff out.json tests/expected.json

clean:
	rm -f $(BIN) $(OBJS) out.json

.PHONY: all test clean
