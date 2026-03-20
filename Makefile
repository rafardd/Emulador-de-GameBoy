# GameBoy Emulator Makefile

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 $(shell sdl2-config --cflags)
LDFLAGS = $(shell sdl2-config --libs)

TARGET = gb_emulator
SRCS = start.c cpu.c ppu.c cycle_cost.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

re: clean all

.PHONY: all clean re
