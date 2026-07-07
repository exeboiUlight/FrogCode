#include "../core/app.h"
#include "platform.h"
#include "../core/colors.h"

#ifdef _WIN32
static HANDLE hConsole;
static DWORD orig_mode;
#endif

void plat_init(void) {
#ifdef _WIN32
    hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleMode(hConsole, &orig_mode);
    SetConsoleMode(hConsole, orig_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_INPUT);
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    SetConsoleTitleA("FrogCode - IDE");
#else
    initscr();
    cbreak();
    noecho();
    nonl();
    keypad(stdscr, TRUE);
    curs_set(1);
    start_color();
    colors_init();
#endif
}

void plat_cleanup(void) {
#ifdef _WIN32
    SetConsoleMode(hConsole, orig_mode);
    system("cls");
#else
    endwin();
#endif
}

void plat_get_size(void) {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    con_w = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    con_h = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
#else
    getmaxyx(stdscr, con_h, con_w);
#endif
}

void plat_goto(int x, int y) {
#ifdef _WIN32
    COORD pos = { (SHORT)x, (SHORT)y };
    SetConsoleCursorPosition(hConsole, pos);
#else
    move(y, x);
#endif
}

void plat_putchar(int c) {
#ifdef _WIN32
    putchar(c);
#else
    addch(c);
#endif
}

void plat_print(const char *s) {
#ifdef _WIN32
    printf("%s", s);
#else
    addstr(s);
#endif
}

void plat_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
#ifdef _WIN32
    vprintf(fmt, args);
#else
    char buf[4096];
    vsnprintf(buf, sizeof(buf), fmt, args);
    addstr(buf);
#endif
    va_end(args);
}

void plat_clear_line(void) {
#ifdef _WIN32
    printf("\033[K");
#else
    clrtoeol();
#endif
}

void plat_refresh(void) {
#ifdef _WIN32
    fflush(stdout);
#else
    refresh();
#endif
}

int plat_getch(void) {
#ifdef _WIN32
    return _getch();
#else
    return getch();
#endif
}

int plat_kbhit(void) {
#ifdef _WIN32
    return _kbhit();
#else
    timeout(0);
    int ch = getch();
    timeout(-1);
    if (ch == ERR) return 0;
    ungetch(ch);
    return 1;
#endif
}

void plat_set_title(const char *title) {
#ifdef _WIN32
    SetConsoleTitleA(title);
#else
    printf("\033]0;%s\007", title);
    fflush(stdout);
#endif
}

void plat_cursor_visible(int vis) {
#ifdef _WIN32
    CONSOLE_CURSOR_INFO ci = { sizeof(ci), vis ? TRUE : FALSE };
    SetConsoleCursorInfo(hConsole, &ci);
#else
    curs_set(vis ? 2 : 0);
#endif
}

int plat_file_exists(const char *path) {
#ifdef _WIN32
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat st;
    return stat(path, &st) == 0;
#endif
}

int plat_is_dir(const char *path) {
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
#endif
}

void plat_mkdir(const char *path) {
#ifdef _WIN32
    CreateDirectoryA(path, NULL);
#else
    mkdir(path, 0755);
#endif
}

void plat_remove(const char *path) {
    remove(path);
}

void plat_get_exe_dir(char *buf, int buf_size) {
#ifdef _WIN32
    GetModuleFileNameA(NULL, buf, buf_size);
    char *slash = strrchr(buf, '\\');
    if (slash) *slash = '\0';
#else
    ssize_t len = readlink("/proc/self/exe", buf, buf_size - 1);
    if (len > 0) {
        buf[len] = '\0';
        char *slash = strrchr(buf, '/');
        if (slash) *slash = '\0';
    } else {
        strcpy(buf, ".");
    }
#endif
}
