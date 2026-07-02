CC      ?= cc
CFLAGS  ?= -O3 -std=c11 -Wall -Wextra -Wpedantic
# For maximum speed on the build host, add: -march=native

BIN     = zdoc-c-parser
OBJS    = c_parser.o main.o

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

c_parser.o: c_parser.c c_parser.h
main.o: main.c c_parser.h

test: $(BIN)
	./$(BIN) tests/sample.c tests/sample.cpp

clean:
	rm -f $(BIN) $(OBJS)

.PHONY: all test clean
