CC = gcc
CFLAGS = -Wall -Wextra -O2 -Wno-format-truncation -Wno-stringop-truncation
LDFLAGS =
SRC = src/main.c src/editor.c src/highlight.c src/platform.c src/filetree.c src/terminal.c src/draw.c src/input.c
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
	@if not exist "%LOCALAPPDATA%\Programs\FrogCode" mkdir "%LOCALAPPDATA%\Programs\FrogCode"
	copy /Y "$(TARGET)" "%LOCALAPPDATA%\Programs\FrogCode\"
	@setx PATH "%PATH%;%LOCALAPPDATA%\Programs\FrogCode" > nul 2>&1
	@echo Installed to %LOCALAPPDATA%\Programs\FrogCode\
else
	mkdir -p /usr/local/bin
	cp $(TARGET) /usr/local/bin/$(TARGET)
	chmod 755 /usr/local/bin/$(TARGET)
endif

.PHONY: all clean run install
