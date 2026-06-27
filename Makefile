CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -D_GNU_SOURCE -Iinclude
LDFLAGS =

SRCS    = $(wildcard src/*.c)
OBJS    = $(SRCS:.c=.o)
TARGET  = flashspeedtest

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

test: $(TARGET)
	@bash test.sh

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean test
