CFLAGS  ?= -O3 -std=c11 -Wall -Wextra -Wpedantic


ifeq ($(OS),Windows_NT)
    EXE := .exe
    CC  ?= gcc
    ifeq ($(shell uname -s 2>NUL),)
        WINDOWS_CMD := 1
    endif
else
    EXE :=
    CC  ?= cc
endif

BIN = zdoc-c-parser$(EXE)

ifdef WINDOWS_CMD
    # Native Windows shell (cmd.exe): del, backslash paths, no ./ prefix.
    RM      = del /Q /F
    RUN     = $(BIN)
    fixpath = $(subst /,\,$1)
else
    # POSIX shell: Linux, macOS, or Git Bash / MSYS on Windows.
    RM      = rm -f
    RUN     = ./$(BIN)
    fixpath = $1
endif

OBJS      = c_parser.o main.o
TEST_BIN  = c_parser_test$(EXE)
TEST_OBJS = tests/c_parser_test.o

SHARED_H = ../shared/parser_shared.h ../../extractor/doc_extractor/module_tree/modtree_tables.h

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

c_parser.o: c_parser.c c_parser.h $(SHARED_H)
main.o: main.c c_parser.h $(SHARED_H)

tests/c_parser_test.o: tests/c_parser_test.c c_parser.h $(SHARED_H)

$(TEST_BIN): c_parser.o $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ c_parser.o $(TEST_OBJS)

test: $(TEST_BIN)
ifdef WINDOWS_CMD
	$(TEST_BIN)
else
	./$(TEST_BIN)
endif

clean:
	-$(RM) $(call fixpath,$(BIN) $(OBJS) $(TEST_BIN) $(TEST_OBJS))

.PHONY: all test clean
