#include "../core/app.h"
#include "../platform/platform.h"
#include "../editor/editor.h"
#include "../editor/highlight.h"
#include "draw.h"
#include "../project/setup.h"
#include "../project/project.h"
#include "../project/hub.h"

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

static int num_width(EditorBuffer *eb) {
    int w = 3;
    for (int n = eb->line_count; n > 0; n /= 10) w--;
    if (w < 3) w = 3;
    return w;
}

void draw_all(void) {
    plat_get_size();
    plat_cursor_visible(0);

    if (mode == MODE_SETUP) {
        setup_draw();
        return;
    }
    if (mode == MODE_PROJECT) {
        project_draw();
        return;
    }
    if (mode == MODE_HUB) {
        hub_draw();
        return;
    }

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
                const char *pname = sidebar_panel == PANEL_FILES ? " FILES " : " SEARCH ";
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
        plat_print(" TERMINAL ");
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
        snprintf(left, sizeof(left), " FILES ");
    else if (mode == MODE_TERMINAL)
        snprintf(left, sizeof(left), " TERMINAL ");
    else if (eb)
        snprintf(left, sizeof(left), " Ln %d, Col %d %s ", eb->cursor_y + 1, eb->cursor_x + 1, eb->modified ? "+" : "");
    else
        snprintf(left, sizeof(left), " Ready ");
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
