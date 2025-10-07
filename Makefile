# Makefile for ls re-engineering project (Feature 1-3)

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -D_DEFAULT_SOURCE

# Directories
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

# Files
SRC = $(SRC_DIR)/ls-v1.2.0.c
OBJ = $(OBJ_DIR)/ls-v1.2.0.o
TARGET = $(BIN_DIR)/ls

# Default rule
all: $(TARGET)

# Link object file to create final executable
$(TARGET): $(OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(OBJ) -o $(TARGET)
	@echo "✅ Build complete! Executable created at $(TARGET)"

# Compile source to object file
$(OBJ): $(SRC)
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $(SRC) -o $(OBJ)
	@echo "🧱 Compiled $(SRC) → $(OBJ)"

# Clean up build files
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "🧹 Cleaned build directories!"

# Run the compiled program
run: all
	./$(TARGET)

.PHONY: all clean run
