CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -I../shared
TARGET  = plx_parser
OBJS    = plx_parser.o str_helpers.o parser_helpers.o plx_parser_demo.o parser_shared_interface.o file_buffer.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

debug: CFLAGS += -g -O0 -fsanitize=address -fno-omit-frame-pointer
debug: $(TARGET)

SHARED_H = ../shared/parser_shared.h ../../extractor/doc_extractor/module_tree/modtree_tables.h

plx_parser.o: plx_parser.c plx_parser.h str_helpers.h parser_helpers.h ../shared/file_buffer.h $(SHARED_H)
file_buffer.o: ../shared/file_buffer.c ../shared/file_buffer.h
	$(CC) $(CFLAGS) -c -o $@ ../shared/file_buffer.c
str_helpers.o: str_helpers.c str_helpers.h
parser_helpers.o: parser_helpers.c parser_helpers.h
plx_parser_demo.o: plx_parser_demo.c plx_parser.h str_helpers.h $(SHARED_H)
parser_shared_interface.o: ../shared/parser_shared_interface.c $(SHARED_H)
	$(CC) $(CFLAGS) -c -o $@ ../shared/parser_shared_interface.c

ifeq ($(OS),Windows_NT)
clean: SHELL := cmd.exe
clean:
	-del /f /q $(subst /,\,$(TARGET).exe $(OBJS)) 2>NUL
else
clean:
	rm -f $(TARGET) $(OBJS)
endif

.PHONY: all clean
