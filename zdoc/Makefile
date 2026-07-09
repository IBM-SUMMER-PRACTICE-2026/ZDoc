# zdoc — CLI entry point (see README.md, docs/ZDOC.md).
#
# Builds the `zdoc` front-end: option/tag parsing, zdoc.yaml loading, and a
# dry-run that discovers the source files the pipeline would process. It reuses
# the module_tree file walker from the extractor; those sources are compiled in
# directly rather than emitting per-object files into another component's dir.

CFLAGS ?= -O2 -std=c11 -Wall -Wextra

# Pick a compiler, but never clobber one the user set explicitly. `?=` is not
# enough here: GNU make ships a built-in default of CC=cc, so `?=` would keep
# `cc` even where only gcc exists (e.g. MinGW). Override only that default.
ifeq ($(OS),Windows_NT)
    EXE := .exe
    ifeq ($(origin CC),default)
        CC := gcc
    endif
    ifeq ($(shell uname -s 2>NUL),)
        WINDOWS_CMD := 1
    endif
else
    EXE :=
    ifeq ($(origin CC),default)
        CC := cc
    endif
endif

BIN = zdoc$(EXE)

# Where `make install` drops the binary. Must be a directory already on PATH.
# Override on the command line, e.g. `make install PREFIX=/usr/local/bin`.
# On Windows `make` is often run from cmd/PowerShell where $(HOME) is empty, so
# fall back to $(USERPROFILE) — otherwise PREFIX would collapse to a bare /bin.
ifeq ($(OS),Windows_NT)
    HOMEDIR := $(if $(HOME),$(HOME),$(USERPROFILE))
else
    HOMEDIR := $(HOME)
endif
PREFIX ?= $(HOMEDIR)/bin

ifdef WINDOWS_CMD
    # Native Windows shell (cmd.exe): del, backslash paths, no ./ prefix.
    RM      = del /Q /F
    RUN     = $(BIN)
    fixpath = $(subst /,\,$1)
    MKDIR_P = if not exist "$(call fixpath,$(PREFIX))" mkdir "$(call fixpath,$(PREFIX))"
    CP      = copy /Y
else
    # POSIX shell: Linux, macOS, or Git Bash / MSYS on Windows.
    RM      = rm -f
    RUN     = ./$(BIN)
    fixpath = $1
    MKDIR_P = mkdir -p "$(PREFIX)"
    CP      = cp -f
endif

MODTREE = ../extractor/doc_extractor/module_tree

SRCS = main.c \
       cli.c \
       config.c \
       glob.c \
       $(MODTREE)/fs_walk.c \
       $(MODTREE)/modtree_table.c

HDRS = cli.h \
       glob.h \
       $(MODTREE)/fs_walk.h \
       $(MODTREE)/modtree_tables.h

all: $(BIN)

$(BIN): $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

# Golden-file test suite (dry-run output + exit codes). Needs a POSIX shell —
# run from Git Bash on Windows. See tests/README.md.
test: $(BIN)
	cd tests && sh run.sh

# Put `zdoc` on PATH. PREFIX must be a directory already on PATH.
install: $(BIN)
	$(MKDIR_P)
	$(CP) $(call fixpath,$(BIN)) $(call fixpath,$(PREFIX))
	@echo "installed $(BIN) -> $(PREFIX)"

uninstall:
	-$(RM) $(call fixpath,$(PREFIX)/$(BIN))

clean:
	-$(RM) $(call fixpath,$(BIN))

.PHONY: all test clean install uninstall
