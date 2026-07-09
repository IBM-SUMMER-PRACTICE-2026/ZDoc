CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Wpedantic

CC ?= cc
RM  = rm -f

# bob_client is a library unit — no binary of its own. The AI-mode CLI and the
# daemon link these objects and supply the snippets.
LIB_OBJS = bob_client.o closure.o util.o

all: $(LIB_OBJS)

bob_client.o: bob_client.c bob_client.h
closure.o:    closure.c closure.h util.h
util.o:       util.c util.h

test: tests/test_closure
	./tests/test_closure

tests/test_closure: tests/test_closure.c closure.c util.c closure.h util.h
	$(CC) $(CFLAGS) -I. -o $@ tests/test_closure.c closure.c util.c

# Manual end-to-end check against the real Bob CLI (needs bob on PATH + a
# session/API key + the zdoc-diagram skill discoverable). Not run by `test`.
live: tests/live_bob
	./tests/live_bob

tests/live_bob: tests/live_bob.c closure.c bob_client.c util.c \
                closure.h bob_client.h util.h
	$(CC) $(CFLAGS) -I. -o $@ tests/live_bob.c closure.c bob_client.c util.c

clean:
	-$(RM) $(LIB_OBJS) tests/test_closure tests/live_bob

.PHONY: all test live clean
