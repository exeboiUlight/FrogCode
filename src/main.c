#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <windows.h>
#include <conio.h>
#include <direct.h>
#include "editor.h"
#include "highlight.h"

#define VERSION "1.0"
#define MAX_TABS 16
#define MAX_PATH_LEN 512
#define MAX_SEARCH_LEN 256
#define SIDEBAR_WIDTH 28
#define MAX_W 300
#define MAX_H 100

typedef enum { MODE_EDITOR, MODE_FILETREE, MODE_TERMINAL, MODE_SEARCH, MODE_COMMAND } AppMode;
typedef enum { PANEL_FILES, PANEL_SEARCH } SidebarPanel;

typedef struct { EditorBuffer buf; } Tab;

static HANDLE hConsole;
static DWORD orig_mode;
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

static char screen_chars[MAX_H][MAX_W];
static WORD screen_attrs[MAX_H][MAX_W];

static void buf_put(int x, int y, char c, WORD attr) {
    if (x >= 0 && x < con_w && y >= 0 && y < con_h) {
        screen_chars[y][x] = c;
        screen_attrs[y][x] = attr;
    }
}

static void buf_str(int x, int y, const char *s, WORD attr) {
    for (int i = 0; s[i]; i++) buf_put(x + i, y, s[i], attr);
}

static void buf_fill(int x, int y, int w, char c, WORD attr) {
    for (int i = 0; i < w; i++) buf_put(x + i, y, c, attr);
}

static void buf_flush(void) {
    COORD bufSize = { (SHORT)con_w, (SHORT)con_h };
    COORD bufPos = { 0, 0 };
    SMALL_RECT rect = { 0, 0, (SHORT)(con_w - 1), (SHORT)(con_h - 1) };
    CHAR_INFO chars[MAX_H][MAX_W];
    for (int y = 0; y < con_h; y++)
        for (int x = 0; x < con_w; x++) {
            chars[y][x].Char.AsciiChar = screen_chars[y][x];
            chars[y][x].Attributes = screen_attrs[y][x];
        }
    WriteConsoleOutputA(hConsole, &chars[0][0], bufSize, bufPos, &rect);
}

static void buf_clear(void) {
    WORD def = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    for (int y = 0; y < con_h; y++)
        for (int x = 0; x < con_w; x++) {
            screen_chars[y][x] = ' ';
            screen_attrs[y][x] = def;
        }
}

static void update_size(void) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    con_w = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    con_h = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    if (con_w > MAX_W) con_w = MAX_W;
    if (con_h > MAX_H) con_h = MAX_H;
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

static void refresh_file_tree(void) {
    file_tree_count = 0;
    file_tree_selected = 0;
    file_tree_scroll = 0;
    scan_directory(work_dir, 0);
}

static void open_file_from_tree(void) {
    if (file_tree_count == 0) return;
    char *path = file_tree_entries[file_tree_selected];
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(path, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    FindClose(h);
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) return;
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
}

/* ==================== CLANGD DOWNLOAD ==================== */

static void download_clangd(void) {
    char check_path[MAX_PATH_LEN];
    snprintf(check_path, sizeof(check_path), "%s\\.frogcode", work_dir);
    DWORD attr = GetFileAttributesA(check_path);
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) return;

    CreateDirectoryA(check_path, NULL);
    snprintf(check_path, sizeof(check_path), "%s\\.frogcode\\clangd", work_dir);
    CreateDirectoryA(check_path, NULL);

    char url[1024];
#ifdef _WIN64
    snprintf(url, sizeof(url), "https://github.com/clangd/clangd/releases/download/22.1.6/clangd-windows-22.1.6.zip");
#elif defined(__APPLE__)
    snprintf(url, sizeof(url), "https://github.com/clangd/clangd/releases/download/22.1.6/clangd-mac-22.1.6.zip");
#else
    snprintf(url, sizeof(url), "https://github.com/clangd/clangd/releases/download/22.1.6/clangd-linux-22.1.6.zip");
#endif

    char zip_path[MAX_PATH_LEN];
    snprintf(zip_path, sizeof(zip_path), "%s\\.frogcode\\clangd.zip", work_dir);

    snprintf(status_msg, sizeof(status_msg), "Скачивание clangd...");
    needs_redraw = 1;

    char ps_cmd[2048];
    snprintf(ps_cmd, sizeof(ps_cmd),
        "powershell -Command \"[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri '%s' -OutFile '%s'\"",
        url, zip_path);
    system(ps_cmd);

    char extract_dir[MAX_PATH_LEN];
    snprintf(extract_dir, sizeof(extract_dir), "%s\\.frogcode\\clangd", work_dir);
    char ps_extract[2048];
    snprintf(ps_extract, sizeof(ps_extract),
        "powershell -Command \"Expand-Archive -Path '%s' -DestinationPath '%s' -Force\"",
        zip_path, extract_dir);
    system(ps_extract);
    DeleteFileA(zip_path);

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
        strcat(exe_path, ".exe");
    } else {
        snprintf(exe_path, sizeof(exe_path), "%s.exe", eb->filepath);
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
        snprintf(build_cmd, sizeof(build_cmd), "python \"%s\"", eb->filepath);
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
    update_size();
    buf_clear();

    WORD title_attr = BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    WORD tab_active = BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    WORD tab_inactive = FOREGROUND_INTENSITY;
    WORD sidebar_attr = BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    WORD sidebar_sel = BACKGROUND_RED | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    WORD line_num_attr = FOREGROUND_INTENSITY;
    WORD def_attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    WORD status_attr = BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;

    char title[128];
    snprintf(title, sizeof(title), " FrogCode v%s ", VERSION);
    int tlen = strlen(title);
    int tpad = (con_w - tlen) / 2;
    if (tpad < 0) tpad = 0;
    buf_fill(0, 0, con_w, ' ', title_attr);
    buf_str(tpad, 0, title, title_attr);

    if (tab_count > 0) {
        int x = 0;
        for (int i = 0; i < tab_count && x < con_w; i++) {
            char tlab[64];
            snprintf(tlab, sizeof(tlab), " %s%s ", tabs[i].buf.name, tabs[i].buf.modified ? "*" : "");
            int tl = strlen(tlab);
            if (x + tl > con_w) tl = con_w - x;
            WORD a = (i == active_tab) ? tab_active : tab_inactive;
            buf_str(x, 1, tlab, a);
            x += tl;
        }
        if (x < con_w) buf_fill(x, 1, con_w - x, ' ', def_attr);
    }

    int term_h = terminal_open ? 6 : 0;
    int editor_h = con_h - 2 - term_h;
    int sidebar_w = sidebar_open ? SIDEBAR_WIDTH : 0;

    if (sidebar_open) {
        buf_fill(0, 2, SIDEBAR_WIDTH, ' ', sidebar_attr);
        const char *pname = sidebar_panel == PANEL_FILES ? " ФАЙЛЫ " : " ПОИСК ";
        buf_str(0, 2, pname, sidebar_attr);

        for (int i = 1; i < editor_h; i++) {
            int idx = i - 1 + file_tree_scroll;
            buf_fill(0, i + 2, SIDEBAR_WIDTH, ' ', sidebar_attr);
            if (idx < file_tree_count) {
                int sel = (idx == file_tree_selected);
                WORD a = sel ? sidebar_sel : sidebar_attr;
                int nlen = strlen(file_tree_names[idx]);
                if (nlen > SIDEBAR_WIDTH) nlen = SIDEBAR_WIDTH;
                buf_str(0, i + 2, file_tree_names[idx], a);
            }
        }
    }

    EditorBuffer *eb = cur_buf();
    int nw = eb ? num_width(eb) : 3;
    int text_w = con_w - sidebar_w - nw - 2;
    if (text_w < 10) text_w = 10;

    for (int i = 0; i < editor_h; i++) {
        buf_fill(sidebar_w, i + 2, con_w - sidebar_w, ' ', def_attr);
        if (!eb) continue;
        int line_idx = i + eb->scroll_y;
        if (line_idx < eb->line_count) {
            char nbuf[16];
            snprintf(nbuf, sizeof(nbuf), "%*d ", nw, line_idx + 1);
            buf_str(sidebar_w, i + 2, nbuf, line_num_attr);

            char *line = eb->lines[line_idx];
            int len = strlen(line);
            int j = eb->scroll_x;
            int px = sidebar_w + nw + 1;
            char wbuf[256];

            while (j < len && (px - sidebar_w - nw - 1) < text_w) {
                char c = line[j];
                if (c == '#') {
                    while (j < len && (px - sidebar_w - nw - 1) < text_w) {
                        buf_put(px, i + 2, line[j], FOREGROUND_GREEN | FOREGROUND_INTENSITY);
                        j++; px++;
                    }
                } else if (c == '"') {
                    buf_put(px, i + 2, c, FOREGROUND_RED | FOREGROUND_INTENSITY);
                    j++; px++;
                    while (j < len && line[j] != '"' && (px - sidebar_w - nw - 1) < text_w) {
                        buf_put(px, i + 2, line[j], FOREGROUND_RED | FOREGROUND_INTENSITY);
                        j++; px++;
                    }
                    if (j < len) { buf_put(px, i + 2, line[j], FOREGROUND_RED | FOREGROUND_INTENSITY); j++; px++; }
                } else if (c == '/' && j + 1 < len && line[j+1] == '/') {
                    while (j < len && (px - sidebar_w - nw - 1) < text_w) {
                        buf_put(px, i + 2, line[j], FOREGROUND_INTENSITY);
                        j++; px++;
                    }
                } else if (isdigit(c)) {
                    int wi = 0;
                    while (j < len && (isalnum(line[j]) || line[j] == '.') && wi < 255) wbuf[wi++] = line[j++];
                    for (int k = 0; k < wi && (px - sidebar_w - nw - 1) < text_w; k++) {
                        buf_put(px, i + 2, wbuf[k], FOREGROUND_RED | FOREGROUND_GREEN);
                        px++;
                    }
                } else if (isalpha(c) || c == '_') {
                    int wi = 0;
                    while (j < len && (isalnum(line[j]) || line[j] == '_') && wi < 255) wbuf[wi++] = line[j++];
                    wbuf[wi] = '\0';
                    int kw = highlight_get_color(wbuf);
                    WORD ka = kw ? kw : def_attr;
                    for (int k = 0; k < wi && (px - sidebar_w - nw - 1) < text_w; k++) {
                        buf_put(px, i + 2, wbuf[k], ka);
                        px++;
                    }
                } else {
                    buf_put(px, i + 2, c, def_attr);
                    j++; px++;
                }
            }
            if (j < len) buf_put(px, i + 2, '>', FOREGROUND_INTENSITY);
        } else {
            buf_str(sidebar_w, i + 2, "~", line_num_attr);
        }
    }

    if (terminal_open) {
        int y0 = con_h - 6;
        buf_fill(0, y0, con_w, ' ', FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        buf_str(0, y0, " ТЕРМИНАЛ ", FOREGROUND_GREEN | FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

        char *lines_buf[64];
        int lc = 0;
        if (terminal_out_len > 0) {
            char tmp[16384];
            int copy_len = terminal_out_len;
            if (copy_len > (int)sizeof(tmp) - 1) copy_len = (int)sizeof(tmp) - 1;
            memcpy(tmp, terminal_out, copy_len);
            tmp[copy_len] = '\0';
            char *ctx = NULL;
            char *tok = strtok_s(tmp, "\n", &ctx);
            while (tok && lc < 64) { lines_buf[lc++] = tok; tok = strtok_s(NULL, "\n", &ctx); }
        }
        int start = lc > 4 ? lc - 4 : 0;
        for (int i = 0; i < 4; i++) {
            buf_fill(0, y0 + 1 + i, con_w, ' ', def_attr);
            if (start + i < lc) {
                int slen = strlen(lines_buf[start + i]);
                if (slen > con_w - 1) slen = con_w - 1;
                char tmpc = lines_buf[start + i][slen];
                lines_buf[start + i][slen] = '\0';
                buf_str(1, y0 + 1 + i, lines_buf[start + i], FOREGROUND_INTENSITY);
                lines_buf[start + i][slen] = tmpc;
            }
        }

        buf_fill(0, con_h - 1, con_w, ' ', def_attr);
        char prompt[4098];
        snprintf(prompt, sizeof(prompt), ">%s", terminal_cmd);
        buf_str(0, con_h - 1, prompt, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    }

    int sy = terminal_open ? con_h - 7 : con_h - 1;
    buf_fill(0, sy, con_w, ' ', status_attr);
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
    buf_str(0, sy, left, status_attr);
    int rlen = strlen(right);
    if (rlen < con_w) buf_str(con_w - rlen, sy, right, status_attr);

    if (input_mode) {
        int iy = con_h - 1;
        buf_fill(0, iy, con_w, ' ', FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        char ibuf[640];
        snprintf(ibuf, sizeof(ibuf), "%s%s", input_prompt, input_buf);
        buf_str(0, iy, ibuf, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    }

    buf_flush();

    if (eb && mode == MODE_EDITOR) {
        ensure_bounds(eb);
        ensure_scroll(eb);
        int dy = eb->cursor_y - eb->scroll_y;
        int dx = eb->cursor_x - eb->scroll_x;
        if (dx < 0) dx = 0;
        COORD pos = { (SHORT)(sidebar_w + nw + 1 + dx), (SHORT)(dy + 2) };
        SetConsoleCursorPosition(hConsole, pos);
    }

    CONSOLE_CURSOR_INFO ci = { sizeof(ci), TRUE };
    SetConsoleCursorInfo(hConsole, &ci);
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
        else if (ch == 8) { if (terminal_cmd_len > 0) terminal_cmd[--terminal_cmd_len] = '\0'; }
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
            case 72: if (file_tree_selected > 0) file_tree_selected--; break;
            case 80: if (file_tree_selected < file_tree_count - 1) file_tree_selected++; break;
            case 13: open_file_from_tree(); break;
            case 'r': refresh_file_tree(); break;
        }
        int vis = con_h - 4;
        if (file_tree_selected < file_tree_scroll) file_tree_scroll = file_tree_selected;
        if (file_tree_selected >= file_tree_scroll + vis) file_tree_scroll = file_tree_selected - vis + 1;
        needs_redraw = 1;
        return;
    }

    int ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    int shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

    if (ch == 0 || ch == 224) {
        int ch2 = _getch();
        EditorBuffer *eb = cur_buf();
        switch (ch2) {
            case 59: start_input("Поиск: "); mode = MODE_SEARCH; return;
            case 68: if (eb) editor_toggle_comment(eb); return;
            case 60: terminal_open = !terminal_open; mode = terminal_open ? MODE_TERMINAL : MODE_EDITOR; needs_redraw = 1; return;
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
            case 9: sidebar_open = !sidebar_open; mode = sidebar_open ? MODE_FILETREE : MODE_EDITOR; if (sidebar_open) refresh_file_tree(); needs_redraw = 1; return;
            case 'K': terminal_open = !terminal_open; mode = terminal_open ? MODE_TERMINAL : MODE_EDITOR; needs_redraw = 1; return;
        }
        return;
    }

    EditorBuffer *eb = cur_buf();
    if (!eb) return;
    switch (ch) {
        case 13: editor_newline(eb); break;
        case 8: editor_backspace(eb); break;
        case 9: editor_tab(eb); break;
        case 3: if (eb->filepath[0]) { editor_save(eb); snprintf(status_msg, sizeof(status_msg), "Сохранено"); } break;
        case 24: editor_delete_line(eb); break;
        default:
            if (ch >= 32 && ch < 127) editor_insert_char(eb, (char)ch);
            break;
    }
    needs_redraw = 1;
}

/* ==================== MAIN ==================== */

static void init_work_dir(void) {
    if (work_dir[0]) return;
    DWORD len = GetCurrentDirectoryA(sizeof(work_dir), work_dir);
    if (len == 0) strcpy(work_dir, ".");
}

static void startup_download_clangd(void) {
    char check[MAX_PATH_LEN];
    snprintf(check, sizeof(check), "%s\\.frogcode", work_dir);
    DWORD attr = GetFileAttributesA(check);
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) return;
    download_clangd();
}

int main(int argc, char *argv[]) {
    hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleMode(hConsole, &orig_mode);
    SetConsoleMode(hConsole, orig_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_INPUT);
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    SetConsoleTitleA("FrogCode - IDE для C/C++");

    CONSOLE_CURSOR_INFO ci = { sizeof(ci), FALSE };
    SetConsoleCursorInfo(hConsole, &ci);

    init_work_dir();
    highlight_init();
    update_size();
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
        if (_kbhit()) {
            int ch = _getch();
            if (ch == 27 && !input_mode && mode == MODE_EDITOR) break;
            process_input(ch);
            needs_redraw = 1;
        } else {
            Sleep(16);
        }
    }

    for (int i = 0; i < tab_count; i++) editor_free(&tabs[i].buf);
    SetConsoleMode(hConsole, orig_mode);
    system("cls");
    return 0;
}
