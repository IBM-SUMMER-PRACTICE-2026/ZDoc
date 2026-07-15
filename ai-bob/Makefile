# ZDoc ai-bob/ layer — process_interface-backed Bob client + test tool.
#
# bob_call_test parses one source file with the real parser to get a Module,
# builds the Bob prompt, then calls bob_call() (which shells out to the real
# Bob binary through process_interface) and prints the raw response.

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

BOB_SRCS = bob-client.c process_interface/process_interface.c

all: bob_call_test

bob_call_test: bob_call_test.c $(BOB_SRCS) bob-client.h process_interface/process_interface.h $(PARSER_SRCS)
	$(CC) $(CFLAGS) -o $@ bob_call_test.c $(BOB_SRCS) $(PARSER_SRCS)

clean:
	-$(RM) bob_call_test

.PHONY: all clean
