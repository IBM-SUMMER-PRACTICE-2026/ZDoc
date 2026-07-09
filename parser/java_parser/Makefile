CC      = gcc
CFLAGS  = -O2 -Wall -Wextra -std=c11 -I../shared
TARGET  = zdoc-java-parser
OBJS    = main.o java_parser.o doc_comment.o parser_shared_interface.o

SHARED_H = ../shared/parser_shared.h ../../extractor/doc_extractor/module_tree/modtree_tables.h

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

main.o: main.c java_parser.h $(SHARED_H)
java_parser.o: java_parser.c java_parser.h util.h doc_comment.h $(SHARED_H)
doc_comment.o: doc_comment.c doc_comment.h util.h java_parser.h $(SHARED_H)
parser_shared_interface.o: ../shared/parser_shared_interface.c $(SHARED_H)
	$(CC) $(CFLAGS) -c -o $@ ../shared/parser_shared_interface.c

ifeq ($(OS),Windows_NT)
clean: SHELL := cmd.exe
clean:
	-del /f /q $(subst /,\,$(TARGET) $(TARGET).exe $(OBJS)) 2>NUL
else
clean:
	rm -f $(TARGET) $(OBJS)
endif

.PHONY: all clean
