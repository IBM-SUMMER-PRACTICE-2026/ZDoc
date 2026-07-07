CC      ?= cc
CFLAGS  ?= -O2 -std=c11 -Wall -Wextra -Wpedantic
LDLIBS   = -lpthread

BIN      = zdoc-bob-client
CORE     = util.o sha256.o json.o closure.o graph.o bob_client.o

all: $(BIN)

$(BIN): $(CORE) main.o
	$(CC) $(CFLAGS) -o $@ $(CORE) main.o $(LDLIBS)

util.o: util.c util.h
sha256.o: sha256.c sha256.h
json.o: json.c json.h util.h
closure.o: closure.c closure.h util.h
graph.o: graph.c graph.h json.h util.h
bob_client.o: bob_client.c bob_client.h graph.h sha256.h util.h
main.o: main.c bob_client.h closure.h json.h util.h

tests/test_closure: tests/test_closure.c $(CORE)
	$(CC) $(CFLAGS) -I. -o $@ tests/test_closure.c $(CORE) $(LDLIBS)

tests/test_graph: tests/test_graph.c $(CORE)
	$(CC) $(CFLAGS) -I. -o $@ tests/test_graph.c $(CORE) $(LDLIBS)

test: $(BIN) tests/test_closure tests/test_graph
	./tests/test_closure
	./tests/test_graph
	sh tests/run_tests.sh

clean:
	rm -f $(BIN) *.o tests/test_closure tests/test_graph

.PHONY: all test clean
