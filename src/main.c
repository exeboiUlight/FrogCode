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
#include "editor.h"
#include "highlight.h"

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

#define VERSION "1.0"
#define MAX_TABS 16
#define MAX_PATH_LEN 512
#define MAX_SEARCH_LEN 256
#define SIDEBAR_WIDTH 28

typedef enum { MODE_EDITOR, MODE_FILETREE, MODE_TERMINAL, MODE_SEARCH, MODE_COMMAND } AppMode;
typedef enum { PANEL_FILES, PANEL_SEARCH } SidebarPanel;

typedef struct { EditorBuffer buf; } Tab;

#ifdef _WIN32
static HANDLE hConsole;
static DWORD orig_mode;
#endif

static int con_w, con_h;
static AppMode mode = MODE_EDITOR;
static SidebarPanel sidebar_panel = PANEL_FILES;
static int sidebar_open = 1;
static int terminal_open = 0;

static Tab tabs[MAX_TABS];
static int tab_count = 0;
static int active_tab = 0;

static char search_query[MAX_SEARCH_LEN] = "";
static int search_active = 0;

static char terminal_cmd[4096] = "";
static int terminal_cmd_len = 0;
static char terminal_out[16384] = "";
static int terminal_out_len = 0;

static char work_dir[MAX_PATH_LEN] = "";
static char file_tree_entries[512][MAX_PATH_LEN];
static char file_tree_names[512][256];
static int file_tree_count = 0;
static int file_tree_selected = 0;
static int file_tree_scroll = 0;

static char status_msg[256] = "";
static char input_buf[512] = "";
static int input_len = 0;
static int input_mode = 0;
static char input_prompt[128] = "";

static int needs_redraw = 1;

/* ==================== PLATFORM ABSTRACTION ==================== */

static void plat_init(void) {
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
    keypad(stdscr, TRUE);
    curs_set(1);
    start_color();
    colors_init();
#endif
}

static void plat_cleanup(void) {
#ifdef _WIN32
    SetConsoleMode(hConsole, orig_mode);
    system("cls");
#else
    endwin();
#endif
}

static void plat_get_size(void) {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    con_w = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    con_h = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
#else
    getmaxyx(stdscr, con_h, con_w);
#endif
}

static void plat_goto(int x, int y) {
#ifdef _WIN32
    COORD pos = { (SHORT)x, (SHORT)y };
    SetConsoleCursorPosition(hConsole, pos);
#else
    move(y, x);
#endif
}

static void plat_putchar(int c) {
#ifdef _WIN32
    putchar(c);
#else
    addch(c);
#endif
}

static void plat_print(const char *s) {
#ifdef _WIN32
    printf("%s", s);
#else
    addstr(s);
#endif
}

static void plat_printf(const char *fmt, ...) {
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

static void plat_clear_line(void) {
#ifdef _WIN32
    printf("\033[K");
#else
    clrtoeol();
#endif
}

static void plat_refresh(void) {
#ifdef _WIN32
    fflush(stdout);
#else
    refresh();
#endif
}

static int plat_getch(void) {
#ifdef _WIN32
    return _getch();
#else
    return getch();
#endif
}

static int plat_kbhit(void) {
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

static void plat_set_title(const char *title) {
#ifdef _WIN32
    SetConsoleTitleA(title);
#endif
}

static void plat_cursor_visible(int vis) {
#ifdef _WIN32
    CONSOLE_CURSOR_INFO ci = { sizeof(ci), vis ? TRUE : FALSE };
    SetConsoleCursorInfo(hConsole, &ci);
#else
    curs_set(vis ? 2 : 0);
#endif
}

static int plat_file_exists(const char *path) {
#ifdef _WIN32
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat st;
    return stat(path, &st) == 0;
#endif
}

static int plat_is_dir(const char *path) {
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
#endif
}

static void plat_mkdir(const char *path) {
#ifdef _WIN32
    CreateDirectoryA(path, NULL);
#else
    mkdir(path, 0755);
#endif
}

static void plat_remove(const char *path) {
    remove(path);
}

static EditorBuffer *cur_buf(void) {
    if (tab_count == 0) return NULL;
    return &tabs[active_tab].buf;
}

static void ensure_line_check(EditorBuffer *eb, int idx) {
    if (idx >= 0 && idx < MAX_LINES && !eb->lines[idx]) {
        eb->lines[idx] = malloc(MAX_LINE_LEN);
        eb->lines[idx][0] = '\0';
    }
}

static void ensure_bounds(EditorBuffer *eb) {
    if (!eb) return;
    if (eb->cursor_y >= eb->line_count) eb->cursor_y = eb->line_count - 1;
    if (eb->cursor_y < 0) eb->cursor_y = 0;
    ensure_line_check(eb, eb->cursor_y);
    int len = strlen(eb->lines[eb->cursor_y]);
    if (eb->cursor_x > len) eb->cursor_x = len;
    if (eb->cursor_x < 0) eb->cursor_x = 0;
}

static void ensure_scroll(EditorBuffer *eb) {
    if (!eb) return;
    int sidebar_w = sidebar_open ? SIDEBAR_WIDTH : 0;
    int term_h = terminal_open ? 6 : 0;
    int editor_h = con_h - 2 - term_h;
    if (editor_h < 1) editor_h = 1;

    int dy = eb->cursor_y - eb->scroll_y;
    if (dy < 0) eb->scroll_y = eb->cursor_y;
    if (dy >= editor_h) eb->scroll_y = eb->cursor_y - editor_h + 1;
    if (eb->scroll_y < 0) eb->scroll_y = 0;

    int nw = 3;
    for (int n = eb->line_count; n > 0; n /= 10) nw--;
    if (nw < 3) nw = 3;
    int text_w = con_w - sidebar_w - nw - 2;
    if (text_w < 10) text_w = 10;

    int dx = eb->cursor_x - eb->scroll_x;
    if (dx < 0) eb->scroll_x = eb->cursor_x;
    if (dx >= text_w) eb->scroll_x = eb->cursor_x - text_w + 1;
    if (eb->scroll_x < 0) eb->scroll_x = 0;
}

/* ==================== FILE TREE ==================== */

#ifdef _WIN32

static void scan_directory(const char *dir, int depth) {
    WIN32_FIND_DATAA fd;
    char pattern[MAX_PATH_LEN];
    snprintf(pattern, sizeof(pattern), "%s\\*", dir);
    HANDLE hFind = FindFirstFileA(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) continue;
        if (file_tree_count >= 512) break;
        char fullpath[MAX_PATH_LEN];
        snprintf(fullpath, sizeof(fullpath), "%s\\%s", dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            snprintf(file_tree_names[file_tree_count], 256, "%*s[+] %s", depth * 2, "", fd.cFileName);
        else
            snprintf(file_tree_names[file_tree_count], 256, "%*s    %s", depth * 2, "", fd.cFileName);
        snprintf(file_tree_entries[file_tree_count], MAX_PATH_LEN, "%s", fullpath);
        file_tree_count++;
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && depth < 3)
            scan_directory(fullpath, depth + 1);
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
}

#else

static void scan_directory(const char *dir, int depth) {
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        if (ent->d_name[0] == '.') continue;
        if (file_tree_count >= 512) break;
        char fullpath[MAX_PATH_LEN];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, ent->d_name);
        int is_dir = 0;
        struct stat st;
        if (stat(fullpath, &st) == 0) is_dir = S_ISDIR(st.st_mode);
        if (is_dir)
            snprintf(file_tree_names[file_tree_count], 256, "%*s[+] %s", depth * 2, "", ent->d_name);
        else
            snprintf(file_tree_names[file_tree_count], 256, "%*s    %s", depth * 2, "", ent->d_name);
        snprintf(file_tree_entries[file_tree_count], MAX_PATH_LEN, "%s", fullpath);
        file_tree_count++;
        if (is_dir && depth < 3) scan_directory(fullpath, depth + 1);
    }
    closedir(d);
}

#endif

static void refresh_file_tree(void) {
    file_tree_count = 0;
    file_tree_selected = 0;
    file_tree_scroll = 0;
    scan_directory(work_dir, 0);
}

static void open_file_from_tree(void) {
    if (file_tree_count == 0) return;
    char *path = file_tree_entries[file_tree_selected];
    if (plat_is_dir(path)) return;
    if (tab_count < MAX_TABS) {
        for (int i = 0; i < tab_count; i++) {
            if (strcmp(tabs[i].buf.filepath, path) == 0) {
                active_tab = i;
                mode = MODE_EDITOR;
                needs_redraw = 1;
                return;
            }
        }
        editor_init(&tabs[tab_count].buf);
        if (editor_load(&tabs[tab_count].buf, path) == 0) {
            active_tab = tab_count;
            tab_count++;
        }
        mode = MODE_EDITOR;
    }
    needs_redraw = 1;
}

/* ==================== TERMINAL ==================== */

static void terminal_execute(const char *cmd) {
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hRead, hWrite;
    CreatePipe(&hRead, &hWrite, &sa, 0);

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));
    char cmdline[4096];
    snprintf(cmdline, sizeof(cmdline), "cmd /c \"%s\"", cmd);

    int ok = CreateProcessA(NULL, cmdline, NULL, NULL, TRUE,
                            CREATE_NO_WINDOW, NULL, work_dir, &si, &pi);
    CloseHandle(hWrite);

    if (ok) {
        char buf[4096];
        DWORD n;
        terminal_out_len = 0;
        while (ReadFile(hRead, buf, sizeof(buf) - 1, &n, NULL) && n > 0) {
            int copy = (int)n;
            if (terminal_out_len + copy > (int)sizeof(terminal_out) - 1)
                copy = (int)sizeof(terminal_out) - 1 - terminal_out_len;
            memcpy(terminal_out + terminal_out_len, buf, copy);
            terminal_out_len += copy;
            terminal_out[terminal_out_len] = '\0';
        }
        WaitForSingleObject(pi.hProcess, 10000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        snprintf(terminal_out, sizeof(terminal_out), "Ошибка запуска процесса");
        terminal_out_len = strlen(terminal_out);
    }
    CloseHandle(hRead);
#else
    char full_cmd[4096];
    snprintf(full_cmd, sizeof(full_cmd), "cd \"%s\" && %s 2>&1", work_dir, cmd);
    FILE *fp = popen(full_cmd, "r");
    if (!fp) {
        snprintf(terminal_out, sizeof(terminal_out), "Ошибка запуска процесса");
        terminal_out_len = strlen(terminal_out);
        return;
    }
    terminal_out_len = 0;
    char buf[4096];
    while (fgets(buf, sizeof(buf), fp)) {
        int len = strlen(buf);
        if (terminal_out_len + len < (int)sizeof(terminal_out) - 1) {
            memcpy(terminal_out + terminal_out_len, buf, len);
            terminal_out_len += len;
            terminal_out[terminal_out_len] = '\0';
        }
    }
    pclose(fp);
#endif
}

/* ==================== CLANGD DOWNLOAD ==================== */

static void download_clangd(void) {
    char check_path[MAX_PATH_LEN];
    snprintf(check_path, sizeof(check_path), "%s/.frogcode", work_dir);
    if (plat_is_dir(check_path)) return;

    plat_mkdir(check_path);

    char url[1024];
#ifdef _WIN64
    snprintf(url, sizeof(url), "https://github.com/clangd/clangd/releases/download/22.1.6/clangd-windows-22.1.6.zip");
#elif defined(__APPLE__)
    snprintf(url, sizeof(url), "https://github.com/clangd/clangd/releases/download/22.1.6/clangd-mac-22.1.6.zip");
#else
    snprintf(url, sizeof(url), "https://github.com/clangd/clangd/releases/download/22.1.6/clangd-linux-22.1.6.zip");
#endif

    char zip_path[MAX_PATH_LEN];
    snprintf(zip_path, sizeof(zip_path), "%s/.frogcode/clangd.zip", work_dir);

    snprintf(status_msg, sizeof(status_msg), "Скачивание clangd...");
    needs_redraw = 1;

#ifdef _WIN32
    char ps_cmd[2048];
    snprintf(ps_cmd, sizeof(ps_cmd),
        "powershell -Command \"[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri '%s' -OutFile '%s'\"",
        url, zip_path);
    system(ps_cmd);

    char extract_dir[MAX_PATH_LEN];
    snprintf(extract_dir, sizeof(extract_dir), "%s\\.frogcode\\clangd", work_dir);
    char ps_extract[2048];
    snprintf(ps_extract, sizeof(ps_extract),
        "powershell -Command \"Expand-Archive -Path '%s' -DestinationPath '%s' -Force'",
        zip_path, extract_dir);
    system(ps_extract);
#else
    char curl_cmd[2048];
    snprintf(curl_cmd, sizeof(curl_cmd), "curl -L '%s' -o '%s'", url, zip_path);
    system(curl_cmd);

    char extract_dir[MAX_PATH_LEN];
    snprintf(extract_dir, sizeof(extract_dir), "%s/.frogcode/clangd", work_dir);
    plat_mkdir(extract_dir);
    char unzip_cmd[2048];
    snprintf(unzip_cmd, sizeof(unzip_cmd), "unzip -o '%s' -d '%s'", zip_path, extract_dir);
    system(unzip_cmd);
#endif

    plat_remove(zip_path);

    snprintf(status_msg, sizeof(status_msg), "clangd установлен");
    needs_redraw = 1;
}

/* ==================== COMPILER ==================== */

static void build_and_run(void) {
    EditorBuffer *eb = cur_buf();
    if (!eb || !eb->filepath[0]) {
        snprintf(status_msg, sizeof(status_msg), "Сначала сохраните файл (Ctrl+S)");
        needs_redraw = 1;
        return;
    }
    if (eb->modified) editor_save(eb);

    char exe_path[MAX_PATH_LEN];
    char *dot = strrchr(eb->filepath, '.');
    if (dot) {
        int len = dot - eb->filepath;
        memcpy(exe_path, eb->filepath, len);
        exe_path[len] = '\0';
#ifdef _WIN32
        strcat(exe_path, ".exe");
#else
        strcat(exe_path, ".out");
#endif
    } else {
        snprintf(exe_path, sizeof(exe_path), "%s.out", eb->filepath);
    }

    terminal_open = 1;
    mode = MODE_TERMINAL;

    char build_cmd[2048];
    const char *ext = dot ? dot : "";
    if (strcmp(ext, ".c") == 0)
        snprintf(build_cmd, sizeof(build_cmd), "gcc -Wall -o \"%s\" \"%s\" 2>&1 && echo === OK === && \"%s\"",
                 exe_path, eb->filepath, exe_path);
    else if (strcmp(ext, ".cpp") == 0 || strcmp(ext, ".cc") == 0)
        snprintf(build_cmd, sizeof(build_cmd), "g++ -Wall -o \"%s\" \"%s\" 2>&1 && echo === OK === && \"%s\"",
                 exe_path, eb->filepath, exe_path);
    else if (strcmp(ext, ".py") == 0)
        snprintf(build_cmd, sizeof(build_cmd), "python3 \"%s\"", eb->filepath);
    else {
        snprintf(status_msg, sizeof(status_msg), "Неподдерживаемый тип файла: %s", ext);
        needs_redraw = 1;
        return;
    }
    terminal_execute(build_cmd);
    needs_redraw = 1;
}

/* ==================== DRAWING ==================== */

static int num_width(EditorBuffer *eb) {
    int w = 3;
    for (int n = eb->line_count; n > 0; n /= 10) w--;
    if (w < 3) w = 3;
    return w;
}

static void draw_all(void) {
    plat_get_size();
    plat_cursor_visible(0);

    plat_goto(0, 0);
    set_color_fgbg(CLR_BLACK, CLR_WHITE);
    char title[128];
    snprintf(title, sizeof(title), " FrogCode v%s ", VERSION);
    int tlen = strlen(title);
    int pad = (con_w - tlen) / 2;
    if (pad < 0) pad = 0;
    for (int i = 0; i < con_w; i++) plat_putchar(' ');
    plat_goto(pad, 0);
    plat_print(title);
    reset_color();

    if (tab_count > 0) {
        plat_goto(0, 1);
        int x = 0;
        for (int i = 0; i < tab_count && x < con_w; i++) {
            set_color_fgbg(i == active_tab ? CLR_BLACK : CLR_GREEN,
                           i == active_tab ? CLR_WHITE : CLR_BLACK);
            char tlab[64];
            snprintf(tlab, sizeof(tlab), " %s%s ", tabs[i].buf.name, tabs[i].buf.modified ? "*" : "");
            int tl = strlen(tlab);
            if (x + tl > con_w) tl = con_w - x;
            plat_printf("%.*s", tl, tlab);
            x += tl;
        }
        while (x < con_w) { plat_putchar(' '); x++; }
        reset_color();
    }

    int term_h = terminal_open ? 6 : 0;
    int editor_h = con_h - 2 - term_h;
    int sidebar_w = sidebar_open ? SIDEBAR_WIDTH : 0;

    if (sidebar_open) {
        for (int i = 0; i < editor_h; i++) {
            plat_goto(0, i + 2);
            if (i == 0) {
                set_color_fgbg(CLR_BLACK, CLR_WHITE);
                const char *pname = sidebar_panel == PANEL_FILES ? " ФАЙЛЫ " : " ПОИСК ";
                char padded[32];
                snprintf(padded, sizeof(padded), "%-28s", pname);
                plat_print(padded);
                reset_color();
            } else if (sidebar_panel == PANEL_FILES) {
                int idx = i - 1 + file_tree_scroll;
                if (idx < file_tree_count) {
                    if (idx == file_tree_selected)
                        set_color_fgbg(CLR_WHITE, CLR_RED);
                    else
                        reset_color();
                    char padded[256];
                    snprintf(padded, sizeof(padded), "%-28s", file_tree_names[idx]);
                    plat_print(padded);
                    reset_color();
                } else {
                    for (int p = 0; p < SIDEBAR_WIDTH; p++) plat_putchar(' ');
                }
            } else {
                for (int p = 0; p < SIDEBAR_WIDTH; p++) plat_putchar(' ');
            }
        }
    }

    EditorBuffer *eb = cur_buf();
    int nw = eb ? num_width(eb) : 3;
    int text_w = con_w - sidebar_w - nw - 2;
    if (text_w < 10) text_w = 10;

    for (int i = 0; i < editor_h; i++) {
        plat_goto(sidebar_w, i + 2);
        reset_color();
        plat_clear_line();
        if (!eb) continue;
        int line_idx = i + eb->scroll_y;
        plat_goto(sidebar_w, i + 2);
        if (line_idx < eb->line_count) {
            set_color(CLR_GREEN);
            plat_printf("%*d ", nw, line_idx + 1);
            char *line = eb->lines[line_idx];
            int len = strlen(line);
            int j = eb->scroll_x;
            int printed = 0;
            char wbuf[256];
            while (j < len && printed < text_w) {
                char c = line[j];
                if (c == '#') {
                    set_color(CLR_GREEN);
                    while (j < len && printed < text_w) { plat_putchar(line[j]); j++; printed++; }
                    reset_color();
                } else if (c == '"') {
                    set_color(CLR_RED);
                    plat_putchar(c); j++; printed++;
                    while (j < len && line[j] != '"' && printed < text_w) { plat_putchar(line[j]); j++; printed++; }
                    if (j < len) { plat_putchar(line[j]); j++; printed++; }
                    reset_color();
                } else if (c == '/' && j + 1 < len && line[j+1] == '/') {
                    set_color(CLR_GREEN);
                    while (j < len && printed < text_w) { plat_putchar(line[j]); j++; printed++; }
                    reset_color();
                } else if (isdigit(c)) {
                    set_color(CLR_YELLOW);
                    while (j < len && (isalnum(line[j]) || line[j] == '.') && printed < text_w) { plat_putchar(line[j]); j++; printed++; }
                    reset_color();
                } else if (isalpha(c) || c == '_') {
                    int wi = 0;
                    while (j < len && (isalnum(line[j]) || line[j] == '_') && wi < 255) wbuf[wi++] = line[j++];
                    wbuf[wi] = '\0';
                    Color kw = highlight_get_color(wbuf);
                    if (kw) set_color(kw);
                    for (int k = 0; k < wi && printed < text_w; k++) { plat_putchar(wbuf[k]); printed++; }
                    if (kw) reset_color();
                } else {
                    plat_putchar(c); j++; printed++;
                }
            }
            if (j < len) { set_color(CLR_GREEN); plat_putchar('>'); reset_color(); }
        } else {
            set_color(CLR_GREEN);
            plat_putchar('~');
        }
    }

    if (terminal_open) {
        int y0 = con_h - 6;
        plat_goto(0, y0);
        set_color(CLR_GREEN);
        plat_print(" ТЕРМИНАЛ ");
        for (int i = 10; i < con_w; i++) plat_putchar(' ');
        reset_color();

        char *lines_buf[64];
        int lc = 0;
        if (terminal_out_len > 0) {
            char tmp[16384];
            int copy_len = terminal_out_len;
            if (copy_len > (int)sizeof(tmp) - 1) copy_len = (int)sizeof(tmp) - 1;
            memcpy(tmp, terminal_out, copy_len);
            tmp[copy_len] = '\0';
#ifdef _WIN32
            char *ctx = NULL;
            char *tok = strtok_s(tmp, "\n", &ctx);
            while (tok && lc < 64) { lines_buf[lc++] = tok; tok = strtok_s(NULL, "\n", &ctx); }
#else
            char *ctx = NULL;
            char *tok = strtok_r(tmp, "\n", &ctx);
            while (tok && lc < 64) { lines_buf[lc++] = tok; tok = strtok_r(NULL, "\n", &ctx); }
#endif
        }
        int start = lc > 4 ? lc - 4 : 0;
        for (int i = 0; i < 4; i++) {
            plat_goto(0, y0 + 1 + i);
            reset_color();
            plat_clear_line();
            if (start + i < lc) {
                set_color(CLR_GREEN);
                int slen = strlen(lines_buf[start + i]);
                if (slen > con_w - 2) slen = con_w - 2;
                char tc = lines_buf[start + i][slen];
                lines_buf[start + i][slen] = '\0';
                plat_printf(" %s", lines_buf[start + i]);
                lines_buf[start + i][slen] = tc;
            }
        }

        plat_goto(0, con_h - 1);
        set_color(CLR_GREEN);
        plat_printf(">%s", terminal_cmd);
        reset_color();
        plat_clear_line();
    }

    int sy = terminal_open ? con_h - 7 : con_h - 1;
    plat_goto(0, sy);
    set_color_fgbg(CLR_BLACK, CLR_WHITE);
    char left[256], right[256];
    if (mode == MODE_FILETREE)
        snprintf(left, sizeof(left), " ФАЙЛЫ ");
    else if (mode == MODE_TERMINAL)
        snprintf(left, sizeof(left), " ТЕРМИНАЛ ");
    else if (eb)
        snprintf(left, sizeof(left), " Ln %d, Col %d %s ", eb->cursor_y + 1, eb->cursor_x + 1, eb->modified ? "+" : "");
    else
        snprintf(left, sizeof(left), " Готово ");
    snprintf(right, sizeof(right), " FrogCode v%s ", VERSION);
    int llen = strlen(left);
    int rlen = strlen(right);
    int spad = con_w - llen - rlen;
    if (spad < 0) spad = 0;
    plat_printf("%s%*s%s", left, spad, "", right);
    reset_color();

    if (input_mode) {
        plat_goto(0, con_h - 1);
        set_color(CLR_GREEN);
        plat_printf("%s%s", input_prompt, input_buf);
        reset_color();
        plat_clear_line();
    }

    if (eb && mode == MODE_EDITOR) {
        ensure_bounds(eb);
        ensure_scroll(eb);
        int dy = eb->cursor_y - eb->scroll_y;
        int dx = eb->cursor_x - eb->scroll_x;
        if (dx < 0) dx = 0;
        plat_goto(sidebar_w + nw + 1 + dx, dy + 2);
    }

    plat_cursor_visible(1);
    plat_refresh();
    needs_redraw = 0;
}

/* ==================== INPUT ==================== */

static void start_input(const char *prompt) {
    input_mode = 1;
    input_buf[0] = '\0';
    input_len = 0;
    snprintf(input_prompt, sizeof(input_prompt), "%s", prompt);
    needs_redraw = 1;
}

static void handle_input_key(int ch) {
    if (ch == 13) { input_buf[input_len] = '\0'; input_mode = 0; return; }
    if (ch == 27) { input_buf[0] = '\0'; input_len = 0; input_mode = 0; return; }
    if (ch == 8) { if (input_len > 0) input_buf[--input_len] = '\0'; needs_redraw = 1; return; }
    if (ch >= 32 && ch < 127 && input_len < (int)sizeof(input_buf) - 1) {
        input_buf[input_len++] = ch;
        input_buf[input_len] = '\0';
    }
    needs_redraw = 1;
}

static void open_file_dialog(void) { start_input("Открыть файл: "); needs_redraw = 1; }

static void save_file_dialog(void) {
    EditorBuffer *eb = cur_buf();
    if (!eb) return;
    if (eb->filepath[0]) {
        editor_save(eb);
        snprintf(status_msg, sizeof(status_msg), "Сохранено: %s", eb->name);
    } else {
        start_input("Сохранить как: ");
    }
    needs_redraw = 1;
}

static void goto_line_dialog(void) { start_input("Перейти к строке: "); needs_redraw = 1; }

static void new_file(void) {
    if (tab_count >= MAX_TABS) return;
    editor_init(&tabs[tab_count].buf);
    active_tab = tab_count;
    tab_count++;
    mode = MODE_EDITOR;
    needs_redraw = 1;
}

static void close_tab(void) {
    if (tab_count <= 0) return;
    editor_free(&tabs[active_tab].buf);
    for (int i = active_tab; i < tab_count - 1; i++) tabs[i] = tabs[i + 1];
    tab_count--;
    if (active_tab >= tab_count) active_tab = tab_count - 1;
    if (tab_count == 0) new_file();
    needs_redraw = 1;
}

static void select_all(void) {
    EditorBuffer *eb = cur_buf();
    if (!eb) return;
    eb->cursor_x = 0; eb->cursor_y = 0;
}

static void process_input(int ch) {
    if (input_mode) {
        handle_input_key(ch);
        if (!input_mode && input_buf[0]) {
            if (strcmp(input_prompt, "Открыть файл: ") == 0) {
                if (tab_count < MAX_TABS) {
                    editor_init(&tabs[tab_count].buf);
                    if (editor_load(&tabs[tab_count].buf, input_buf) == 0) {
                        active_tab = tab_count; tab_count++;
                    }
                }
            } else if (strcmp(input_prompt, "Сохранить как: ") == 0) {
                EditorBuffer *eb = cur_buf();
                if (eb) { editor_set_file(eb, input_buf); editor_save(eb); }
            } else if (strcmp(input_prompt, "Перейти к строке: ") == 0) {
                EditorBuffer *eb = cur_buf();
                if (eb) editor_goto_line(eb, atoi(input_buf));
            } else if (strcmp(input_prompt, "Поиск: ") == 0) {
                snprintf(search_query, sizeof(search_query), "%s", input_buf);
                search_active = (search_query[0] != '\0');
            }
        }
        needs_redraw = 1;
        return;
    }

    if (mode == MODE_TERMINAL) {
        if (ch == 13 && terminal_cmd_len > 0) {
            terminal_cmd[terminal_cmd_len] = '\0';
            terminal_execute(terminal_cmd);
            terminal_cmd_len = 0; terminal_cmd[0] = '\0';
        } else if (ch == 27) { mode = MODE_EDITOR; terminal_cmd_len = 0; }
#ifdef _WIN32
        else if (ch == 8) { if (terminal_cmd_len > 0) terminal_cmd[--terminal_cmd_len] = '\0'; }
#else
        else if (ch == KEY_BACKSPACE) { if (terminal_cmd_len > 0) terminal_cmd[--terminal_cmd_len] = '\0'; }
#endif
        else if (ch >= 32 && ch < 127 && terminal_cmd_len < (int)sizeof(terminal_cmd) - 1) {
            terminal_cmd[terminal_cmd_len++] = ch;
            terminal_cmd[terminal_cmd_len] = '\0';
        }
        needs_redraw = 1;
        return;
    }

    if (mode == MODE_FILETREE) {
        switch (ch) {
            case 27: case 'q': mode = MODE_EDITOR; break;
            case KEY_UP: case 'k': if (file_tree_selected > 0) file_tree_selected--; break;
            case KEY_DOWN: case 'j': if (file_tree_selected < file_tree_count - 1) file_tree_selected++; break;
#ifdef _WIN32
            case 13: open_file_from_tree(); break;
#else
            case KEY_ENTER: open_file_from_tree(); break;
#endif
            case 'r': refresh_file_tree(); break;
        }
        int vis = con_h - 4;
        if (file_tree_selected < file_tree_scroll) file_tree_scroll = file_tree_selected;
        if (file_tree_selected >= file_tree_scroll + vis) file_tree_scroll = file_tree_selected - vis + 1;
        needs_redraw = 1;
        return;
    }

#ifdef _WIN32
    if (ch == 0 || ch == 224) {
        int ch2 = _getch();
        EditorBuffer *eb = cur_buf();
        switch (ch2) {
            case 59: start_input("Поиск: "); mode = MODE_SEARCH; return;
            case 68: if (eb) editor_toggle_comment(eb); return;
            case 60: terminal_open = !terminal_open; mode = terminal_open ? MODE_TERMINAL : MODE_EDITOR; return;
            case 63: build_and_run(); return;
            case 71: if (eb) eb->cursor_x = 0; return;
            case 79: if (eb) eb->cursor_x = strlen(eb->lines[eb->cursor_y]); return;
            case 72: if (eb) eb->cursor_y--; return;
            case 80: if (eb) eb->cursor_y++; return;
            case 75: if (eb) eb->cursor_x--; return;
            case 77: if (eb) eb->cursor_x++; return;
            case 73: if (eb) eb->cursor_y -= 5; return;
            case 81: if (eb) eb->cursor_y += 5; return;
            case 83: if (eb) editor_delete_forward(eb); return;
        }
        return;
    }
#else
    EditorBuffer *eb = cur_buf();
    switch (ch) {
        case KEY_F(1): start_input("Поиск: "); mode = MODE_SEARCH; return;
        case KEY_F(3): terminal_open = !terminal_open; mode = terminal_open ? MODE_TERMINAL : MODE_EDITOR; needs_redraw = 1; return;
        case KEY_F(5): build_and_run(); return;
        case KEY_UP: if (eb) eb->cursor_y--; return;
        case KEY_DOWN: if (eb) eb->cursor_y++; return;
        case KEY_LEFT: if (eb) eb->cursor_x--; return;
        case KEY_RIGHT: if (eb) eb->cursor_x++; return;
        case KEY_PPAGE: if (eb) eb->cursor_y -= 5; return;
        case KEY_NPAGE: if (eb) eb->cursor_y += 5; return;
        case KEY_HOME: if (eb) eb->cursor_x = 0; return;
        case KEY_END: if (eb) eb->cursor_x = strlen(eb->lines[eb->cursor_y]); return;
        case KEY_DC: if (eb) editor_delete_forward(eb); return;
    }
#endif

    int ctrl = 0;
    int shift = 0;
#ifdef _WIN32
    ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
#else
    if (ch >= 1 && ch <= 26) { ctrl = 1; ch = ch + 'a' - 1; }
#endif

    if (ctrl) {
        EditorBuffer *eb = cur_buf();
        switch (tolower(ch)) {
            case 's': save_file_dialog(); return;
            case 'o': open_file_dialog(); return;
            case 'n': new_file(); return;
            case 'w': close_tab(); return;
            case 'a': select_all(); return;
            case 'g': goto_line_dialog(); return;
            case 'f': start_input("Поиск: "); mode = MODE_SEARCH; return;
            case 'd': if (eb) editor_duplicate_line(eb); return;
            case 'b': sidebar_open = !sidebar_open; return;
            case '`': terminal_open = !terminal_open; mode = terminal_open ? MODE_TERMINAL : MODE_EDITOR; return;
            case 'l': if (eb) editor_move_line_down(eb); return;
            case 'j': if (eb) editor_move_line_up(eb); return;
        }
        return;
    }

    if (shift) {
        switch (ch) {
            case '\t': sidebar_open = !sidebar_open; mode = sidebar_open ? MODE_FILETREE : MODE_EDITOR; if (sidebar_open) refresh_file_tree(); needs_redraw = 1; return;
            case 'K': terminal_open = !terminal_open; mode = terminal_open ? MODE_TERMINAL : MODE_EDITOR; needs_redraw = 1; return;
        }
        return;
    }

    EditorBuffer *eb2 = cur_buf();
    if (!eb2) return;
    switch (ch) {
#ifdef _WIN32
        case 13: editor_newline(eb2); break;
        case 8: editor_backspace(eb2); break;
#else
        case KEY_ENTER: editor_newline(eb2); break;
        case KEY_BACKSPACE: editor_backspace(eb2); break;
#endif
        case '\t': editor_tab(eb2); break;
        case 3: if (eb2->filepath[0]) { editor_save(eb2); snprintf(status_msg, sizeof(status_msg), "Сохранено"); } break;
        case 24: editor_delete_line(eb2); break;
        default:
            if (ch >= 32 && ch < 127) editor_insert_char(eb2, (char)ch);
            break;
    }
    needs_redraw = 1;
}

/* ==================== MAIN ==================== */

static void init_work_dir(void) {
    if (work_dir[0]) return;
#ifdef _WIN32
    DWORD len = GetCurrentDirectoryA(sizeof(work_dir), work_dir);
    if (len == 0) strcpy(work_dir, ".");
#else
    if (!getcwd(work_dir, sizeof(work_dir))) strcpy(work_dir, ".");
#endif
}

static void startup_download_clangd(void) {
    char check[MAX_PATH_LEN];
    snprintf(check, sizeof(check), "%s/.frogcode", work_dir);
    if (plat_is_dir(check)) return;
    download_clangd();
}

int main(int argc, char *argv[]) {
    plat_init();
    plat_set_title("FrogCode - IDE");
    highlight_init();
    plat_get_size();
    new_file();

    if (argc > 1 && argv[1]) {
        char *p = argv[1];
        if (p[0] == '"' || p[0] == '\'') p++;
        int len = strlen(p);
        if (len > 0 && (p[len-1] == '"' || p[len-1] == '\'')) p[len-1] = '\0';
        editor_free(&tabs[active_tab].buf);
        if (editor_load(&tabs[active_tab].buf, p) == 0)
            snprintf(status_msg, sizeof(status_msg), "Открыт: %s", tabs[active_tab].buf.name);
    }

    snprintf(status_msg, sizeof(status_msg), "FrogCode v%s | Shift+Tab: Файлы | Shift+K: Терминал | F5: Запуск", VERSION);
    startup_download_clangd();

    needs_redraw = 1;
    while (1) {
        if (needs_redraw) draw_all();
        if (plat_kbhit()) {
            int ch = plat_getch();
            if (ch == 27 && !input_mode && mode == MODE_EDITOR) break;
            process_input(ch);
            needs_redraw = 1;
        } else {
#ifdef _WIN32
            Sleep(16);
#else
            usleep(16000);
#endif
        }
    }

    for (int i = 0; i < tab_count; i++) editor_free(&tabs[i].buf);
    plat_cleanup();
    return 0;
}
