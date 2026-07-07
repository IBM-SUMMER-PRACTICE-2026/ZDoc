CC     ?= cc
CFLAGS ?= -O2 -Wall -Wextra -std=c11 -I.

BIN     = zdoc-doc-extractor
OBJS    = src/json_read.o src/child_parser.o src/doc_extractor.o src/dx_write.o src/main.o \
          module_tree/fs_walk.o module_tree/modtree_table.o

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

src/json_read.o: src/json_read.c src/json_read.h
src/child_parser.o: src/child_parser.c src/child_parser.h src/doc_extractor.h src/json_read.h
src/doc_extractor.o: src/doc_extractor.c src/doc_extractor.h src/child_parser.h src/json_read.h module_tree/fs_walk.h module_tree/modtree_tables.h
src/dx_write.o: src/dx_write.c src/doc_extractor.h
src/main.o: src/main.c src/doc_extractor.h
module_tree/fs_walk.o: module_tree/fs_walk.c module_tree/fs_walk.h module_tree/modtree_tables.h
module_tree/modtree_table.o: module_tree/modtree_table.c module_tree/modtree_tables.h

test: $(BIN)
	rm -rf out.json
	./$(BIN) --parser-dir tests/fake_parsers tests/fixture_project > out.json
	diff out.json tests/expected.json

clean:
	rm -f $(BIN) $(OBJS) out.json

.PHONY: all test clean
