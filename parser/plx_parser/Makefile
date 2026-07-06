CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra
TARGET  = plx_parser
OBJS    = plx_parser.o helpers.o plx_parser_demo.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

plx_parser.o: plx_parser.c plx_parser.h helpers.h
helpers.o: helpers.c helpers.h
plx_parser_demo.o: plx_parser_demo.c plx_parser.h helpers.h

ifeq ($(OS),Windows_NT)
clean: SHELL := cmd.exe
clean:
	-del /f /q $(subst /,\,$(TARGET) $(OBJS)) 2>NUL
else
clean:
	rm -f $(TARGET) $(OBJS)
endif

.PHONY: all clean
