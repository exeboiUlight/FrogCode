CC = gcc
CFLAGS = -Wall -Wextra -O2 -Wno-format-truncation -Wno-stringop-truncation
LDFLAGS =
TARGET = frogcode

ifeq ($(OS),Windows_NT)
	TARGET := $(TARGET).exe
	LDFLAGS += -luser32
endif

ifndef ($(OS))
	LDFLAGS += $(shell pkg-config --libs ncurses 2>/dev/null)
endif

SRC = src/main.c src/editor.c src/highlight.c src/platform.c src/filetree.c src/terminal.c src/draw.c src/input.c src/setup.c src/project.c src/hub.c
OBJ = $(SRC:.c=.o)
DEP = $(OBJ:.o=.d)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

clean:
ifeq ($(OS),Windows_NT)
	-del /Q $(TARGET) 2>nul
	-del /Q src\*.o 2>nul
	-del /Q src\*.d 2>nul
else
	rm -f $(TARGET) $(OBJ) $(DEP)
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

-include $(DEP)

.PHONY: all clean run install
