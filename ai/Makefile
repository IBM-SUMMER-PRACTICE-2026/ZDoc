# ZDoc ai/ layer — local per-file diagram prompt tool.
#
# zdoc_ai parses one source file with the real parser to get each function and
# its starting line, then prints the Bob prompt + that function list. Bob reads
# the file itself; nothing here shells out to Bob.

CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Wpedantic
CC ?= cc
RM  = rm -f

P  = ../parser
MT = ../extractor/doc_extractor/module_tree

# Real parser library objects (the .c the parsers compile, minus their own
# standalone main()/demo files).
PARSER_SRCS = $(P)/parser_interface.c \
              $(P)/plx_parser/plx_parser.c $(P)/plx_parser/str_helpers.c \
              $(P)/plx_parser/parser_helpers.c \
              $(P)/c_parser/c_parser.c \
              $(P)/java_parser/java_parser.c $(P)/java_parser/doc_comment.c \
              $(P)/shared/parser_shared_interface.c $(P)/shared/file_buffer.c \
              $(MT)/modtree_table.c

all: zdoc_ai

# annotate.c is the online-mode step (run Bob, fill Symbol.diagram by start
# line); it does not parse. zdoc_ai.c is the standalone front end that parses
# one file to feed it. The daemon/CLI would link annotate.c and call
# zdoc_ai_annotate on modules it already parsed (see the PR).
zdoc_ai: zdoc_ai.c annotate.c annotate.h $(PARSER_SRCS)
	$(CC) $(CFLAGS) -o $@ zdoc_ai.c annotate.c $(PARSER_SRCS)

clean:
	-$(RM) zdoc_ai

.PHONY: all clean
