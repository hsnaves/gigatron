# makefile for building the emulator

# General definitions

CC := gcc
RM := rm -f

CFLAGS := -Wall -Wextra -O3
LDFLAGS :=

INCLUDES :=
LIBS := -lm -lSDL2

OBJS :=

TARGET := gtemu

all: $(TARGET)

include module.mk

# Pattern rules

gtemu: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	$(RM) $(TARGET) $(OBJS)

.PHONY: all clean
