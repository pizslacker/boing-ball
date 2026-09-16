# Compiler and flags
CC = gcc
# Added IMG_INIT, IMG_Quit calls means we must link against SDL2_image
CFLAGS = -Wall -Wextra -pedantic -O2 $(shell pkg-config --cflags sdl2 SDL2_image)

# Linker flags (math and SDL2 with extension libraries)
LDLIBS = $(shell pkg-config --libs sdl2 SDL2_image SDL2_mixer) -lm

# Project files
TARGET = sdl2_oop
SRCS = oo.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean run