CFLAGS  ?= -O3 -std=c11 -Wall -Wextra -Wpedantic -I../shared


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

OBJS    = c_parser.o main.o parser_shared_interface.o

SHARED_H = ../shared/parser_shared.h ../../extractor/doc_extractor/module_tree/modtree_tables.h

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

debug: CFLAGS += -g -O0 -fsanitize=address -fno-omit-frame-pointer
debug: $(BIN)

c_parser.o: c_parser.c c_parser.h $(SHARED_H)
main.o: main.c c_parser.h $(SHARED_H)
parser_shared_interface.o: ../shared/parser_shared_interface.c $(SHARED_H)
	$(CC) $(CFLAGS) -c -o $@ ../shared/parser_shared_interface.c

test: $(BIN)
	$(RUN) tests/sample.c tests/sample.cpp

clean:
	-$(RM) $(call fixpath,$(BIN) $(OBJS))

.PHONY: all test debug clean
