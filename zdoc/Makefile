# zdoc — CLI entry point (see README.md, docs/ZDOC.md).
#
# The daemon links three sources from sibling components, so the sources live
# in several directories. We compile them in a single step rather than emit
# per-object files, which would otherwise land inside those other components'
# directories.

# -g emits debug symbols for lldb/gdb; -O0 keeps the code un-reordered so the
# debugger's line stepping and variable inspection match the source.
CFLAGS ?= -g -O0 -std=c11 -Wall -Wextra

ifeq ($(OS),Windows_NT)
    EXE := .exe
    CC  ?= gcc
    # Windows threads come from windows.h; no extra link flag needed.
    THREADFLAGS :=
    ifeq ($(shell uname -s 2>NUL),)
        WINDOWS_CMD := 1
    endif
else
    EXE :=
    CC  ?= cc
    # POSIX threads (pthread) require -pthread at compile and link time.
    THREADFLAGS := -pthread
endif

BIN = zdoc$(EXE)

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

SHARED  = ../parser/shared
MODTREE = ../extractor/doc_extractor/module_tree
PLX     = ../parser/plx_parser
CPARSER = ../parser/c_parser
JPARSER = ../parser/java_parser
PARSER  = ../parser
THREAD  = ./threading_interface

SRCS = zdoc_daemon.c \
       $(PARSER)/parser_interface.c \
       $(SHARED)/parser_shared.c \
       $(MODTREE)/fs_walk.c \
       $(MODTREE)/path_interface.c \
       $(MODTREE)/modtree_table.c \
       $(PLX)/plx_parser.c \
       $(PLX)/helpers.c \
       $(CPARSER)/c_parser.c \
       $(JPARSER)/java_parser.c \
       $(JPARSER)/doc_comment.c \
       $(THREAD)/threading_interface.c

HDRS = $(PARSER)/parser_interface.h \
       $(SHARED)/parser_shared.h \
       $(MODTREE)/fs_walk.h \
       $(MODTREE)/path_interface.h \
       $(MODTREE)/modtree_tables.h \
       $(PLX)/plx_parser.h \
       $(PLX)/helpers.h \
       $(CPARSER)/c_parser.h \
       $(JPARSER)/java_parser.h \
       $(JPARSER)/doc_comment.h \
       $(JPARSER)/util.h \
       $(THREAD)/threading_interface.h

all: $(BIN)

$(BIN): $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(THREADFLAGS)

debug: CFLAGS += -fsanitize=address -fno-omit-frame-pointer
debug: $(BIN)

# Smoke test: walk this directory. The daemon returns 0 on a successful walk.
test: $(BIN)
	$(RUN) .

clean:
	-$(RM) $(call fixpath,$(BIN))

.PHONY: all test debug clean
