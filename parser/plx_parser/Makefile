CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra
TARGET  = plx_parser.exe
OBJS    = plx_parser.o helpers.o plx_parser_demo.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

plx_parser.o: plx_parser.c plx_parser.h helpers.h
helpers.o: helpers.c helpers.h
plx_parser_demo.o: plx_parser_demo.c plx_parser.h helpers.h

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean
