CFLAGS ?= -O2 -std=c11 -Wall -Wextra

ifeq ($(OS),Windows_NT)
    EXE := .exe
    # make's built-in default is CC=cc, which MinGW doesn't ship.
    ifeq ($(origin CC),default)
        CC = gcc
    endif
    # Git Bash / MSYS exports MSYSTEM; plain cmd.exe / PowerShell doesn't.
    # Checking the variable avoids probing the shell, which prints noise.
    ifndef MSYSTEM
        WINDOWS_CMD := 1
    endif
    # Windows threads come from windows.h; no extra link flag needed.
    THREADFLAGS :=
else
    EXE :=
    # POSIX threads (pthread), needed by the daemon's thread pool.
    THREADFLAGS := -pthread
endif

BIN  = zdoc$(EXE)
OBJS = main.o options.o cli.o config.o request.o

# Unit tests link the CLI objects (everything but main.o) straight into the
# test binary, same as the renderer tests - no scripts, no golden files.
TEST_BIN  = cli_test$(EXE)
TEST_OBJS = tests/cli_test.o
LIB_OBJS  = options.o cli.o config.o request.o
TEST_DIR  = zdoc-test-tmp

# The daemon engine lives in sibling components (parser/, extractor/,
# ../threading_interface). Compiled straight from source into the final
# link, like ../Makefile does, rather than emitting .o files into those
# other directories.
SHARED   = ../../parser/shared
MODTREE  = ../../extractor/doc_extractor/module_tree
PLX      = ../../parser/plx_parser
CPARSER  = ../../parser/c_parser
JPARSER  = ../../parser/java_parser
PARSER   = ../../parser
THREAD   = ../threading_interface
RENDERIF = ../renderer_interface
MDREND   = ../../renderer/md_renderer/src
HTMLREND = ../../renderer/html_renderer

DAEMON_SRCS = ../zdoc_daemon.c \
              $(PARSER)/parser_interface.c \
              $(SHARED)/parser_shared.c \
              $(SHARED)/parser_shared_interface.c \
              $(SHARED)/file_buffer.c \
              $(MODTREE)/fs_walk.c \
              $(MODTREE)/path_interface.c \
              $(MODTREE)/modtree_table.c \
              $(PLX)/plx_parser.c \
              $(PLX)/parser_helpers.c \
              $(PLX)/str_helpers.c \
              $(CPARSER)/c_parser.c \
              $(JPARSER)/java_parser.c \
              $(JPARSER)/doc_comment.c \
              $(THREAD)/threading_interface.c \
              $(RENDERIF)/renderer_interface.c \
              $(MDREND)/md_render.c \
              $(HTMLREND)/html_render.c

DAEMON_HDRS = ../zdoc_daemon.h \
              $(PARSER)/parser_interface.h \
              $(SHARED)/parser_shared.h \
              $(SHARED)/file_buffer.h \
              $(MODTREE)/fs_walk.h \
              $(MODTREE)/path_interface.h \
              $(MODTREE)/modtree_tables.h \
              $(PLX)/plx_parser.h \
              $(PLX)/parser_helpers.h \
              $(PLX)/str_helpers.h \
              $(CPARSER)/c_parser.h \
              $(JPARSER)/java_parser.h \
              $(JPARSER)/doc_comment.h \
              $(JPARSER)/util.h \
              $(THREAD)/threading_interface.h \
              $(RENDERIF)/renderer_interface.h \
              $(MDREND)/md_renderer.h \
              $(HTMLREND)/html_renderer.h

# Where `make install` drops the binary. Must be a directory already on PATH.
# Override on the command line, e.g. `make install PREFIX=/usr/local/bin`.
# On Windows `make` is often run from cmd/PowerShell where $(HOME) is empty,
# so fall back to $(USERPROFILE).
ifeq ($(OS),Windows_NT)
    HOMEDIR := $(if $(HOME),$(HOME),$(USERPROFILE))
else
    HOMEDIR := $(HOME)
endif
PREFIX ?= $(HOMEDIR)/bin

ifdef WINDOWS_CMD
    RM      = del /Q /F
    RUN     = $(BIN)
    fixpath = $(subst /,\,$1)
    MKDIR_P = if not exist "$(call fixpath,$(PREFIX))" mkdir "$(call fixpath,$(PREFIX))"
    CP      = copy /Y
else
    RM      = rm -f
    RUN     = ./$(BIN)
    fixpath = $1
    MKDIR_P = mkdir -p "$(PREFIX)"
    CP      = cp -f
endif

all: $(BIN)

$(BIN): $(OBJS) $(DAEMON_SRCS) $(DAEMON_HDRS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(DAEMON_SRCS) $(THREADFLAGS)

main.o:    main.c options.h cli.h config.h request.h ../zdoc_daemon.h
options.o: options.c options.h
cli.o:     cli.c cli.h options.h
config.o:  config.c config.h options.h
request.o: request.c request.h options.h

tests/cli_test.o: tests/cli_test.c cli.h options.h config.h request.h

$(TEST_BIN): $(LIB_OBJS) $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $(LIB_OBJS) $(TEST_OBJS)

test: $(TEST_BIN)
ifdef WINDOWS_CMD
	-if exist $(TEST_DIR) rmdir /S /Q $(TEST_DIR)
	$(TEST_BIN) $(TEST_DIR)
else
	rm -rf $(TEST_DIR)
	./$(TEST_BIN) $(TEST_DIR)
endif

# Put `zdoc` on PATH. PREFIX must be a directory already on PATH.
install: $(BIN)
	$(MKDIR_P)
	$(CP) $(call fixpath,$(BIN)) $(call fixpath,$(PREFIX))
	@echo "installed $(BIN) -> $(PREFIX)"

uninstall:
	-$(RM) $(call fixpath,$(PREFIX)/$(BIN))

clean:
	-$(RM) $(call fixpath,$(BIN) $(OBJS) $(TEST_BIN) $(TEST_OBJS))
ifdef WINDOWS_CMD
	-if exist $(TEST_DIR) rmdir /S /Q $(TEST_DIR)
else
	rm -rf $(TEST_DIR)
endif

.PHONY: all test clean install uninstall
