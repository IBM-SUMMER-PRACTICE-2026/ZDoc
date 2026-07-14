CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Wpedantic

CC ?= cc
RM  = rm -f

# The bc_spawn_capture seam has one implementation per platform. Native Windows
# (where $(OS) is Windows_NT and there is no fork) builds spawn_win.c; every
# POSIX host — Linux, macOS, and fork-providing layers like Cygwin — builds
# spawn_posix.c. Override with `make SPAWN_SRC=spawn_win.c` if autodetection is
# wrong for your toolchain (e.g. cross-compiling).
ifeq ($(OS),Windows_NT)
    SPAWN_SRC ?= spawn_win.c
else
    SPAWN_SRC ?= spawn_posix.c
endif
SPAWN_OBJ = $(SPAWN_SRC:.c=.o)

# bob_client is a library unit — no binary of its own. The AI-mode CLI and the
# daemon link these objects and supply the snippets.
LIB_OBJS = bob_client.o closure.o util.o $(SPAWN_OBJ)

all: $(LIB_OBJS)

bob_client.o: bob_client.c bob_client.h spawn.h
closure.o:    closure.c closure.h util.h
util.o:       util.c util.h
spawn_posix.o: spawn_posix.c spawn.h
spawn_win.o:   spawn_win.c spawn.h

test: tests/test_closure
	./tests/test_closure

tests/test_closure: tests/test_closure.c closure.c util.c closure.h util.h
	$(CC) $(CFLAGS) -I. -o $@ tests/test_closure.c closure.c util.c

# Manual end-to-end check against the real Bob CLI (needs bob on PATH + a
# session/API key + the zdoc-diagram skill discoverable). Not run by `test`.
live: tests/live_bob
	./tests/live_bob

tests/live_bob: tests/live_bob.c closure.c bob_client.c util.c $(SPAWN_SRC) \
                closure.h bob_client.h util.h spawn.h
	$(CC) $(CFLAGS) -I. -o $@ tests/live_bob.c closure.c bob_client.c util.c \
	    $(SPAWN_SRC)

clean:
	-$(RM) $(LIB_OBJS) spawn_posix.o spawn_win.o \
	    tests/test_closure tests/live_bob

.PHONY: all test live clean
