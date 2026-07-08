CC     ?= cc
CFLAGS ?= -O2 -Wall -Wextra -std=c11 -I.

BIN     = zdoc-md-renderer
OBJS    = src/json.o src/md_model.o src/md_render.o src/main.o

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

src/json.o: src/json.c src/json.h
src/md_model.o: src/md_model.c src/md_renderer.h src/json.h
src/md_render.o: src/md_render.c src/md_renderer.h
src/main.o: src/main.c src/md_renderer.h

test: $(BIN)
	rm -rf zdoc-out-test
	./$(BIN) --out-dir ./zdoc-out-test --title "Sample Project" tests/sample.json
	diff -r zdoc-out-test tests/expected

clean:
	rm -f $(BIN) $(OBJS)
	rm -rf zdoc-out-test

.PHONY: all test clean
