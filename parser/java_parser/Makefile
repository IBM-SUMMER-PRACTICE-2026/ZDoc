CC     ?= cc
CFLAGS ?= -O2 -Wall -Wextra -std=c11

SHARED_H = ../shared/parser_shared.h ../../extractor/doc_extractor/module_tree/modtree_tables.h

all: zdoc-java-parser

main.o: main.c java_parser.h $(SHARED_H)
	$(CC) $(CFLAGS) -c main.c -o main.o

java_parser.o: java_parser.c java_parser.h util.h doc_comment.h $(SHARED_H)
	$(CC) $(CFLAGS) -c java_parser.c -o java_parser.o

doc_comment.o: doc_comment.c doc_comment.h util.h java_parser.h $(SHARED_H)
	$(CC) $(CFLAGS) -c doc_comment.c -o doc_comment.o

zdoc-java-parser: main.o java_parser.o doc_comment.o
	$(CC) $(CFLAGS) main.o java_parser.o doc_comment.o -o zdoc-java-parser

clean:
	rm -f zdoc-java-parser *.o
