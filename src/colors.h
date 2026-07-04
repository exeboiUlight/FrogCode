#ifndef COLORS_H
#define COLORS_H

#ifdef _WIN32

#include <windows.h>

typedef enum {
    CLR_DEFAULT = 0,
    CLR_BLACK,
    CLR_RED,
    CLR_GREEN,
    CLR_YELLOW,
    CLR_BLUE,
    CLR_MAGENTA,
    CLR_CYAN,
    CLR_WHITE
} Color;

static inline void colors_init(void) {}

static inline void set_color(Color color)
{
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    WORD attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;

    switch (color) {
    case CLR_BLACK:   attr = 0; break;
    case CLR_RED:     attr = FOREGROUND_RED | FOREGROUND_INTENSITY; break;
    case CLR_GREEN:   attr = FOREGROUND_GREEN | FOREGROUND_INTENSITY; break;
    case CLR_YELLOW:  attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY; break;
    case CLR_BLUE:    attr = FOREGROUND_BLUE | FOREGROUND_INTENSITY; break;
    case CLR_MAGENTA: attr = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY; break;
    case CLR_CYAN:    attr = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY; break;
    case CLR_WHITE:   attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY; break;
    default:          attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; break;
    }

    SetConsoleTextAttribute(h, attr);
}

static inline void set_color_fgbg(Color fg, Color bg)
{
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    WORD attr = 0;

    switch (fg) {
    case CLR_RED:     attr |= FOREGROUND_RED; break;
    case CLR_GREEN:   attr |= FOREGROUND_GREEN; break;
    case CLR_YELLOW:  attr |= FOREGROUND_RED | FOREGROUND_GREEN; break;
    case CLR_BLUE:    attr |= FOREGROUND_BLUE; break;
    case CLR_MAGENTA: attr |= FOREGROUND_RED | FOREGROUND_BLUE; break;
    case CLR_CYAN:    attr |= FOREGROUND_GREEN | FOREGROUND_BLUE; break;
    case CLR_WHITE:   attr |= FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; break;
    default: break;
    }
    switch (bg) {
    case CLR_BLACK:   break;
    case CLR_RED:     attr |= BACKGROUND_RED | BACKGROUND_INTENSITY; break;
    case CLR_GREEN:   attr |= BACKGROUND_GREEN; break;
    case CLR_YELLOW:  attr |= BACKGROUND_RED | BACKGROUND_GREEN; break;
    case CLR_BLUE:    attr |= BACKGROUND_BLUE | BACKGROUND_INTENSITY; break;
    case CLR_MAGENTA: attr |= BACKGROUND_RED | BACKGROUND_BLUE; break;
    case CLR_CYAN:    attr |= BACKGROUND_GREEN | BACKGROUND_BLUE; break;
    case CLR_WHITE:   attr |= BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_INTENSITY; break;
    default: break;
    }

    SetConsoleTextAttribute(h, attr);
}

static inline void reset_color(void)
{
    set_color(CLR_DEFAULT);
}

#else

#include <ncurses.h>

typedef enum {
    CLR_DEFAULT = 0,
    CLR_BLACK,
    CLR_RED,
    CLR_GREEN,
    CLR_YELLOW,
    CLR_BLUE,
    CLR_MAGENTA,
    CLR_CYAN,
    CLR_WHITE
} Color;

static inline void set_color(Color color)
{
    attrset(A_NORMAL);
    switch (color) {
    case CLR_RED:     attron(COLOR_PAIR(2)); break;
    case CLR_GREEN:   attron(COLOR_PAIR(1)); break;
    case CLR_YELLOW:  attron(COLOR_PAIR(3)); break;
    case CLR_CYAN:    attron(COLOR_PAIR(4)); break;
    case CLR_WHITE:   attron(COLOR_PAIR(5)); break;
    case CLR_BLUE:    attron(COLOR_PAIR(6)); break;
    case CLR_MAGENTA: attron(COLOR_PAIR(7)); break;
    default: break;
    }
}

static inline void reset_color(void)
{
    attrset(A_NORMAL);
}

static inline void set_color_fgbg(Color fg, Color bg)
{
    reset_color();

    if (bg == CLR_BLUE) {
        attron(COLOR_PAIR(8));
    } else if (bg == CLR_GREEN && fg == CLR_BLACK) {
        attron(COLOR_PAIR(9));
    } else if (bg == CLR_GREEN && fg == CLR_WHITE) {
        attron(COLOR_PAIR(12));
    } else if (bg == CLR_RED) {
        attron(COLOR_PAIR(10));
    } else if (bg == CLR_GREEN && fg == CLR_GREEN) {
        attron(COLOR_PAIR(11));
    } else if (bg == CLR_BLACK) {
        set_color(fg);
    }
}

static inline void colors_init(void)
{
    init_pair(1, COLOR_GREEN, COLOR_BLACK);
    init_pair(2, COLOR_RED, COLOR_BLACK);
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);
    init_pair(4, COLOR_CYAN, COLOR_BLACK);
    init_pair(5, COLOR_WHITE, COLOR_BLACK);
    init_pair(6, COLOR_BLUE, COLOR_BLACK);
    init_pair(7, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(8, COLOR_WHITE, COLOR_BLUE);
    init_pair(9, COLOR_BLACK, COLOR_GREEN);
    init_pair(10, COLOR_WHITE, COLOR_RED);
    init_pair(11, COLOR_GREEN, COLOR_GREEN);
    init_pair(12, COLOR_WHITE, COLOR_GREEN);
}

#endif

#endif
