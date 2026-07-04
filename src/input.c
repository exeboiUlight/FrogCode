#include "app.h"
#include "platform.h"
#include "editor.h"
#include "filetree.h"
#include "terminal.h"
#include "input.h"
#include "setup.h"
#include "project.h"
#include "hub.h"

void start_input(const char *prompt) {
    input_mode = 1;
    input_buf[0] = '\0';
    input_len = 0;
    snprintf(input_prompt, sizeof(input_prompt), "%s", prompt);
    needs_redraw = 1;
}

void handle_input_key(int ch) {
    if (ch == 13) { input_buf[input_len] = '\0'; input_mode = 0; return; }
    if (ch == 27) { input_buf[0] = '\0'; input_len = 0; input_mode = 0; return; }
    if (ch == 8) { if (input_len > 0) input_buf[--input_len] = '\0'; needs_redraw = 1; return; }
    if (ch >= 32 && ch < 127 && input_len < (int)sizeof(input_buf) - 1) {
        input_buf[input_len++] = ch;
        input_buf[input_len] = '\0';
    }
    needs_redraw = 1;
}

void open_file_dialog(void) { start_input("Open file: "); needs_redraw = 1; }

void save_file_dialog(void) {
    EditorBuffer *eb = cur_buf();
    if (!eb) return;
    if (eb->filepath[0]) {
        editor_save(eb);
        snprintf(status_msg, sizeof(status_msg), "Saved: %s", eb->name);
    } else {
        start_input("Save as: ");
    }
    needs_redraw = 1;
}

void goto_line_dialog(void) { start_input("Go to line: "); needs_redraw = 1; }

void new_file(void) {
    if (tab_count >= MAX_TABS) return;
    editor_init(&tabs[tab_count].buf);
    active_tab = tab_count;
    tab_count++;
    mode = MODE_EDITOR;
    needs_redraw = 1;
}

void close_tab(void) {
    if (tab_count <= 0) return;
    editor_free(&tabs[active_tab].buf);
    for (int i = active_tab; i < tab_count - 1; i++) tabs[i] = tabs[i + 1];
    tab_count--;
    if (active_tab >= tab_count) active_tab = tab_count - 1;
    if (tab_count == 0) new_file();
    needs_redraw = 1;
}

void select_all(void) {
    EditorBuffer *eb = cur_buf();
    if (!eb) return;
    eb->cursor_x = 0; eb->cursor_y = 0;
}

void process_input(int ch) {
    if (input_mode) {
        handle_input_key(ch);
        if (!input_mode && input_buf[0]) {
            if (strcmp(input_prompt, "Open file: ") == 0) {
                if (tab_count < MAX_TABS) {
                    editor_init(&tabs[tab_count].buf);
                    if (editor_load(&tabs[tab_count].buf, input_buf) == 0) {
                        active_tab = tab_count; tab_count++;
                    }
                }
            } else if (strcmp(input_prompt, "Save as: ") == 0) {
                EditorBuffer *eb = cur_buf();
                if (eb) { editor_set_file(eb, input_buf); editor_save(eb); }
            } else if (strcmp(input_prompt, "Go to line: ") == 0) {
                EditorBuffer *eb = cur_buf();
                if (eb) editor_goto_line(eb, atoi(input_buf));
            } else if (strcmp(input_prompt, "Search: ") == 0) {
                snprintf(search_query, sizeof(search_query), "%s", input_buf);
                search_active = (search_query[0] != '\0');
            }
        }
        needs_redraw = 1;
        return;
    }

    if (mode == MODE_SETUP) {
        setup_input(ch);
        return;
    }

    if (mode == MODE_PROJECT) {
        project_input(ch);
        return;
    }

    if (mode == MODE_HUB) {
        hub_input(ch);
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
            case 59: start_input("Search: "); mode = MODE_SEARCH; return;
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
        case KEY_F(1): start_input("Search: "); mode = MODE_SEARCH; return;
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
    if (ch >= 1 && ch <= 26 && ch != '\t' && ch != '\r' && ch != '\n') { ctrl = 1; ch = ch + 'a' - 1; }
#endif

    if (ctrl) {
        EditorBuffer *eb = cur_buf();
        switch (tolower(ch)) {
            case 's': save_file_dialog(); return;
            case 'o': open_file_dialog(); return;
            case 'n': new_file(); return;
            case 'p': project_init(); mode = MODE_PROJECT; return;
            case 'h': hub_init(); mode = MODE_HUB; return;
            case 'w': close_tab(); return;
            case 'a': select_all(); return;
            case 'g': goto_line_dialog(); return;
            case 'f': start_input("Search: "); mode = MODE_SEARCH; return;
            case 'd': if (eb) editor_duplicate_line(eb); return;
            case 'b': sidebar_open = !sidebar_open; return;
            case '`': terminal_open = !terminal_open; mode = terminal_open ? MODE_TERMINAL : MODE_EDITOR; return;
            case 'l': if (eb) editor_move_line_down(eb); return;
            case 'j': if (eb) editor_move_line_up(eb); return;
        }
        return;
    }

#ifndef _WIN32
    if (ch == KEY_BTAB) {
        sidebar_open = !sidebar_open;
        mode = sidebar_open ? MODE_FILETREE : MODE_EDITOR;
        if (sidebar_open) refresh_file_tree();
        needs_redraw = 1;
        return;
    }
#endif

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
        case '\r': case '\n': case KEY_ENTER: editor_newline(eb2); break;
        case KEY_BACKSPACE: editor_backspace(eb2); break;
#endif
        case '\t': editor_tab(eb2); break;
        case 3: if (eb2->filepath[0]) { editor_save(eb2); snprintf(status_msg, sizeof(status_msg), "Saved"); } break;
        case 24: editor_delete_line(eb2); break;
        default:
            if (ch >= 32 && ch < 127) editor_insert_char(eb2, (char)ch);
            break;
    }
    needs_redraw = 1;
}
