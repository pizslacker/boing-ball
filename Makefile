# Compiler settings
CC = gcc
CFLAGS = -O3 -Wall $(shell sdl2-config --cflags)
LDFLAGS = $(shell sdl2-config --libs) -lm

# Files
TARGET = boing-ball
SRC = boing-ball.c
OBJ = $(SRC:.c=.o)

# Default target
all: $(TARGET)

# Link the executable
$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)
	strip $(TARGET)

# Compile object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Convenience rule to build and run
run: $(TARGET)
	./$(TARGET)

# Clean up build files
clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all run clean