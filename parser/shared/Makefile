CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11

MODTREE = ../../extractor/doc_extractor/module_tree

TEST1_BIN  = global_resources_test
TEST1_OBJS = global_resources_test.o parser_shared.o $(MODTREE_OBJ)

TEST2_BIN  = parser_shared_interface_test
TEST2_OBJS = tests/parser_shared_interface_test.o parser_shared_interface.o parser_shared.o $(MODTREE_OBJ)

MODTREE_OBJ = modtree_table.o

modtree_table.o: $(MODTREE)/modtree_table.c $(MODTREE)/modtree_tables.h
	$(CC) $(CFLAGS) -c -o $@ $(MODTREE)/modtree_table.c

parser_shared.o: parser_shared.c parser_shared.h
parser_shared_interface.o: parser_shared_interface.c parser_shared.h
global_resources_test.o: global_resources_test.c parser_shared.h
tests/parser_shared_interface_test.o: tests/parser_shared_interface_test.c parser_shared.h

$(TEST1_BIN): $(TEST1_OBJS)
	$(CC) $(CFLAGS) -o $@ $(TEST1_OBJS)

$(TEST2_BIN): $(TEST2_OBJS)
	$(CC) $(CFLAGS) -o $@ $(TEST2_OBJS)

test: $(TEST1_BIN) $(TEST2_BIN)
	./$(TEST1_BIN)
	./$(TEST2_BIN)

clean:
	rm -f $(TEST1_BIN) $(TEST2_BIN) *.o tests/*.o

.PHONY: test clean
