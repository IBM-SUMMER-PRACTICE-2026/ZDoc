CC      = gcc
CFLAGS  = -O2 -Wall -Wextra -std=c11
TARGET  = zdoc-java-parser
OBJS    = main.o java_parser.o doc_comment.o

TEST_BIN  = java_parser_test
TEST_OBJS = tests/java_parser_test.o

SHARED_H = ../shared/parser_shared.h ../../extractor/doc_extractor/module_tree/modtree_tables.h

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

main.o: main.c java_parser.h $(SHARED_H)
java_parser.o: java_parser.c java_parser.h util.h doc_comment.h $(SHARED_H)
doc_comment.o: doc_comment.c doc_comment.h util.h java_parser.h $(SHARED_H)

tests/java_parser_test.o: tests/java_parser_test.c java_parser.h $(SHARED_H)

$(TEST_BIN): java_parser.o doc_comment.o $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ java_parser.o doc_comment.o $(TEST_OBJS)

test: $(TEST_BIN)
ifeq ($(OS),Windows_NT)
	$(TEST_BIN)
else
	./$(TEST_BIN)
endif

ifeq ($(OS),Windows_NT)
clean: SHELL := cmd.exe
clean:
	-del /f /q $(subst /,\,$(TARGET) $(TARGET).exe $(OBJS) $(TEST_BIN) $(TEST_BIN).exe $(TEST_OBJS)) 2>NUL
else
clean:
	rm -f $(TARGET) $(OBJS) $(TEST_BIN) $(TEST_OBJS)
endif

.PHONY: all test clean
