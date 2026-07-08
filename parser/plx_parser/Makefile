CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra
TARGET  = plx_parser
OBJS    = plx_parser.o helpers.o plx_parser_demo.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

SHARED_H = ../shared/parser_shared.h ../../extractor/doc_extractor/module_tree/modtree_tables.h

plx_parser.o: plx_parser.c plx_parser.h helpers.h $(SHARED_H)
helpers.o: helpers.c helpers.h
plx_parser_demo.o: plx_parser_demo.c plx_parser.h helpers.h $(SHARED_H)

ifeq ($(OS),Windows_NT)
clean: SHELL := cmd.exe
clean:
	-del /f /q $(subst /,\,$(TARGET) $(OBJS)) 2>NUL
else
clean:
	rm -f $(TARGET) $(OBJS)
endif

.PHONY: all clean
