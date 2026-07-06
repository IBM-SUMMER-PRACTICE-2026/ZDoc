CC     ?= cc
CFLAGS ?= -O2 -Wall -Wextra -std=c11

all: zdoc-java-parser

main.o: main.c java_parser.h
	$(CC) $(CFLAGS) -c main.c -o main.o

java_parser.o: java_parser.c java_parser.h util.h doc_comment.h
	$(CC) $(CFLAGS) -c java_parser.c -o java_parser.o

doc_comment.o: doc_comment.c doc_comment.h util.h java_parser.h
	$(CC) $(CFLAGS) -c doc_comment.c -o doc_comment.o

zdoc-java-parser: main.o java_parser.o doc_comment.o
	$(CC) $(CFLAGS) main.o java_parser.o doc_comment.o -o zdoc-java-parser

clean:
	rm -f zdoc-java-parser *.o
