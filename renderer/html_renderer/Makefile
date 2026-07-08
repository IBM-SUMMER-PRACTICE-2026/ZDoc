CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -g

SRCS = main.c html_render.c html_model.c json.c
HDRS = html_renderer.h html_model.h json.h

zdoc-html-renderer: $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

test:
	sh tests/run_tests.sh

clean:
	rm -f zdoc-html-renderer zdoc-html-renderer.exe
	rm -rf tests/tmp

.PHONY: test clean
