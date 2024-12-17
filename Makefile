# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -pthread -g -lm

# Files
SRC = main.c
OBJ = $(SRC:.c=.o)
EXEC = hw2

# Targets
all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(EXEC)

# Optional target to rebuild
rebuild: clean all

.PHONY: all clean rebuild
