# Define the executable name.
EXE = shell
# Define the compiler to use.
CC = gcc
# Collect all .c source files in the directory.
SRC = $(wildcard *.c)
# Compiler flags: enable all warnings, debugging symbols, and optimize at level 3.
CFLAGS = -Wall -g -O3
# Linker flags: enable all warnings.
LDFLAGS = -Wall
# Convert .c filenames to .o.
OBJ= $(SRC:.c=.o)

# Declare 'all' as a phony target to ensure it always runs.
.PHONY : all

# Default target: build all object files and the executable.
all : $(OBJ) $(EXE)

# Rule to link the executable: depends on object files.
$(EXE) : $(OBJ)
	$(CC) $^ -o $@  # Compile command.

# Rule to compile object files from source files.
%.o : %.c
	$(CC) $(CFLAGS) -c $^ -o $@  # Compilation command.