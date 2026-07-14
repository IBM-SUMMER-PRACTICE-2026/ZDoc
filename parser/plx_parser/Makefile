CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra
TARGET  = plx_parser
OBJS    = plx_parser.o helpers.o plx_parser_demo.o

TEST_BIN  = plx_parser_test
TEST_OBJS = tests/plx_parser_test.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

SHARED_H = ../shared/parser_shared.h ../../extractor/doc_extractor/module_tree/modtree_tables.h

plx_parser.o: plx_parser.c plx_parser.h helpers.h $(SHARED_H)
helpers.o: helpers.c helpers.h
plx_parser_demo.o: plx_parser_demo.c plx_parser.h helpers.h $(SHARED_H)

tests/plx_parser_test.o: tests/plx_parser_test.c plx_parser.h helpers.h $(SHARED_H)

$(TEST_BIN): plx_parser.o helpers.o $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ plx_parser.o helpers.o $(TEST_OBJS)

test: $(TEST_BIN)
ifeq ($(OS),Windows_NT)
	$(TEST_BIN)
else
	./$(TEST_BIN)
endif

ifeq ($(OS),Windows_NT)
clean: SHELL := cmd.exe
clean:
	-del /f /q $(subst /,\,$(TARGET) $(OBJS) $(TEST_BIN) $(TEST_BIN).exe $(TEST_OBJS)) 2>NUL
else
clean:
	rm -f $(TARGET) $(OBJS) $(TEST_BIN) $(TEST_OBJS)
endif

.PHONY: all test clean
