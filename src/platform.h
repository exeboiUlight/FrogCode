#ifndef PLATFORM_H
#define PLATFORM_H

#ifndef _WIN32
#include <ncurses.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#include <direct.h>
#define KEY_UP       72
#define KEY_DOWN     80
#define KEY_LEFT     75
#define KEY_RIGHT    77
#define KEY_HOME     71
#define KEY_END      79
#define KEY_PPAGE    73
#define KEY_NPAGE    81
#define KEY_DC       83
#define KEY_BACKSPACE 8
#define KEY_ENTER    13
#define KEY_F(n)     (256 + n)
#endif

#ifndef _WIN32
#ifndef KEY_UP
#define KEY_UP       0
#endif
#ifndef KEY_DOWN
#define KEY_DOWN     0
#endif
#ifndef KEY_LEFT
#define KEY_LEFT     0
#endif
#ifndef KEY_RIGHT
#define KEY_RIGHT    0
#endif
#ifndef KEY_HOME
#define KEY_HOME     0
#endif
#ifndef KEY_END
#define KEY_END      0
#endif
#ifndef KEY_PPAGE
#define KEY_PPAGE    0
#endif
#ifndef KEY_NPAGE
#define KEY_NPAGE    0
#endif
#ifndef KEY_DC
#define KEY_DC       0
#endif
#ifndef KEY_BACKSPACE
#define KEY_BACKSPACE 8
#endif
#ifndef KEY_ENTER
#define KEY_ENTER    13
#endif
#ifndef KEY_F
#define KEY_F(n)     (256 + n)
#endif
#endif

void plat_init(void);
void plat_cleanup(void);
void plat_get_size(void);
void plat_goto(int x, int y);
void plat_putchar(int c);
void plat_print(const char *s);
void plat_printf(const char *fmt, ...);
void plat_clear_line(void);
void plat_refresh(void);
int  plat_getch(void);
int  plat_kbhit(void);
void plat_set_title(const char *title);
void plat_cursor_visible(int vis);
int  plat_file_exists(const char *path);
int  plat_is_dir(const char *path);
void plat_mkdir(const char *path);
void plat_remove(const char *path);

#endif
