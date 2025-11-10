
CC = gcc

CFLAGS = -g -Wall -Wextra -std=c11 -O2

SRC = fio_simulator.c

TARGET = fio_simulator

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean