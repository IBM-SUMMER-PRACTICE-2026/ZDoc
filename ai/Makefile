# ZDoc ai/ layer — the AI Assisted (online) mode orchestration.
#
# ai_mode.o is the object the CLI/daemon links to run online mode; it composes
# the bob_client library unit under bob_client/. `make live` builds a standalone
# end-to-end check of the whole interface (zdoc_ai_annotate) against the real
# Bob CLI — no daemon required.

CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Wpedantic
CC ?= cc
RM  = rm -f

BC = bob_client
MT = ../extractor/doc_extractor/module_tree

# Match bob_client's per-platform spawn selection.
ifeq ($(OS),Windows_NT)
    SPAWN_SRC ?= $(BC)/spawn_win.c
else
    SPAWN_SRC ?= $(BC)/spawn_posix.c
endif

BC_SRCS = $(BC)/bob_client.c $(BC)/closure.c $(BC)/util.c $(SPAWN_SRC)
MT_SRCS = $(MT)/modtree_table.c $(MT)/fs_walk.c $(MT)/path_interface.c

# Real parser library objects (the .c the parsers compile, minus their own
# standalone main()/demo files). zdoc_ai links these so it can parse a real
# file into symbols + starting lines before diagramming each one.
P = ../parser
PARSER_SRCS = $(P)/parser_interface.c \
              $(P)/plx_parser/plx_parser.c $(P)/plx_parser/str_helpers.c \
              $(P)/plx_parser/parser_helpers.c \
              $(P)/c_parser/c_parser.c \
              $(P)/java_parser/java_parser.c $(P)/java_parser/doc_comment.c \
              $(P)/shared/parser_shared_interface.c $(P)/shared/file_buffer.c

all: ai_mode.o

ai_mode.o: ai_mode.c ai_mode.h $(BC)/bob_client.h $(BC)/closure.h $(BC)/util.h
	$(CC) $(CFLAGS) -c -o $@ ai_mode.c

# The local developer tool: diagram one real source file, per symbol, keyed by
# starting line. Links the real parser so `parse_file` yields the symbols.
zdoc_ai: zdoc_ai.c ai_mode.c ai_mode.h $(BC_SRCS) $(MT_SRCS) $(PARSER_SRCS)
	$(CC) $(CFLAGS) -o $@ zdoc_ai.c ai_mode.c $(BC_SRCS) $(MT_SRCS) $(PARSER_SRCS)

# Real-bob end-to-end check of the whole AI-mode interface (needs bob on PATH +
# a valid session/API key; the diagram contract ships in the prompt, so no Bob
# extension is required). Not run by CI.
live: ai_mode_live
	./ai_mode_live

ai_mode_live: ai_mode_live.c ai_mode.c ai_mode.h $(BC_SRCS) $(MT_SRCS)
	$(CC) $(CFLAGS) -o $@ ai_mode_live.c ai_mode.c $(BC_SRCS) $(MT_SRCS)

clean:
	-$(RM) ai_mode.o ai_mode_live

.PHONY: all live clean
