CC = gcc
CFLAGS = -Wall -Wextra -O2 -Wno-format-truncation -Wno-stringop-truncation
LDFLAGS =
SRC = src/main.c src/editor.c src/highlight.c
TARGET = frogcode

ifeq ($(OS),Windows_NT)
	TARGET := $(TARGET).exe
	LDFLAGS += -luser32
else
	TARGET := $(TARGET)
	LDFLAGS += $(shell pkg-config --libs ncurses)
endif

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
ifeq ($(OS),Windows_NT)
	del /Q $(TARGET) 2>nul
else
	rm -f $(TARGET)
endif

run: $(TARGET)
ifeq ($(OS),Windows_NT)
	.\$(TARGET)
else
	./$(TARGET)
endif

install: $(TARGET)
ifeq ($(OS),Windows_NT)
	export PATH := ./
else
    mkdir -p $(PREFIX)/bin
    cp $(TARGET) $(PREFIX)/bin/$(TARGET)
    chmod 755 $(PREFIX)/bin/$(TARGET)
endif

.PHONY: all clean run intall
