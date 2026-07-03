all: zdoc_java.out

main.o: main.c java_parser.h
	gcc -Wall -Wextra -std=c11 -c main.c -o main.o

java_parser.o: java_parser.c java_parser.h util.h doc_comment.h
	gcc -Wall -Wextra -std=c11 -c java_parser.c -o java_parser.o

doc_comment.o: doc_comment.c doc_comment.h util.h java_parser.h
	gcc -Wall -Wextra -std=c11 -c doc_comment.c -o doc_comment.o

zdoc_java.out: main.o java_parser.o doc_comment.o
	gcc main.o java_parser.o doc_comment.o -o zdoc_java.out

clean:
	rm -f *.out *.o
