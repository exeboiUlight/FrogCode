CC = gcc
CFLAGS = -Wall -Wextra -O2 -Wno-format-truncation -Wno-stringop-truncation
LDFLAGS = -luser32
SRC = src/main.c src/editor.c src/highlight.c
TARGET = frogcode.exe

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	del /Q $(TARGET) 2>nul

run: $(TARGET)
	.\$(TARGET)

.PHONY: all clean run
