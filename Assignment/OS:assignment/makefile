# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -Wextra -pthread

# Source files
SRC = bank.c

# Executable name
TARGET = bank

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)