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

SRC = src/core/main.c src/editor/editor.c src/editor/highlight.c src/platform/platform.c src/ui/filetree.c src/ui/terminal.c src/ui/draw.c src/ui/input.c src/project/setup.c src/project/project.c src/project/hub.c
OBJ = $(SRC:.c=.o)
DEP = $(OBJ:.o=.d)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

src/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

clean:
ifeq ($(OS),Windows_NT)
	-del /Q $(TARGET) 2>nul
	-del /Q src\core\*.o 2>nul
	-del /Q src\core\*.d 2>nul
	-del /Q src\editor\*.o 2>nul
	-del /Q src\editor\*.d 2>nul
	-del /Q src\ui\*.o 2>nul
	-del /Q src\ui\*.d 2>nul
	-del /Q src\platform\*.o 2>nul
	-del /Q src\platform\*.d 2>nul
	-del /Q src\project\*.o 2>nul
	-del /Q src\project\*.d 2>nul
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
