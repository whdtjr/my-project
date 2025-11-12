
CC = gcc

CFLAGS = -g -Wall -Wextra -std=c11 -O2

SRC = meta_control.c

TARGET = meta_control

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean