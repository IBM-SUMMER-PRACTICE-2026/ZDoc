CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Wpedantic

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
else
    EXE :=
endif

BIN  = zdoc$(EXE)
OBJS = main.o options.o cli.o config.o request.o

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

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

main.o:    main.c options.h cli.h config.h request.h
options.o: options.c options.h
cli.o:     cli.c cli.h options.h
config.o:  config.c config.h options.h
request.o: request.c request.h options.h

test: $(BIN)
ifdef WINDOWS_CMD
	$(RUN) --version
else
	sh tests/run_tests.sh
endif

# Put `zdoc` on PATH. PREFIX must be a directory already on PATH.
install: $(BIN)
	$(MKDIR_P)
	$(CP) $(call fixpath,$(BIN)) $(call fixpath,$(PREFIX))
	@echo "installed $(BIN) -> $(PREFIX)"

uninstall:
	-$(RM) $(call fixpath,$(PREFIX)/$(BIN))

clean:
	-$(RM) $(call fixpath,$(BIN) $(OBJS))

.PHONY: all test clean install uninstall
