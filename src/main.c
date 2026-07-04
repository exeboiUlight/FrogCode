#include "app.h"
#include "platform.h"
#include "editor.h"
#include "highlight.h"
#include "filetree.h"
#include "terminal.h"
#include "draw.h"
#include "input.h"

int con_w, con_h;
AppMode mode = MODE_EDITOR;
SidebarPanel sidebar_panel = PANEL_FILES;
int sidebar_open = 1;
int terminal_open = 0;

Tab tabs[MAX_TABS];
int tab_count = 0;
int active_tab = 0;

char search_query[MAX_SEARCH_LEN] = "";
int search_active = 0;

char terminal_cmd[4096] = "";
int terminal_cmd_len = 0;
char terminal_out[16384] = "";
int terminal_out_len = 0;

char work_dir[MAX_PATH_LEN] = "";
char file_tree_entries[512][MAX_PATH_LEN];
char file_tree_names[512][256];
int file_tree_count = 0;
int file_tree_selected = 0;
int file_tree_scroll = 0;

char status_msg[256] = "";
char input_buf[512] = "";
int input_len = 0;
int input_mode = 0;
char input_prompt[128] = "";

int needs_redraw = 1;

EditorBuffer *cur_buf(void) {
    if (tab_count == 0) return NULL;
    return &tabs[active_tab].buf;
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
            snprintf(status_msg, sizeof(status_msg), "Opened: %s", tabs[active_tab].buf.name);
    }

    snprintf(status_msg, sizeof(status_msg), "FrogCode v%s | Shift+Tab: Files | Shift+K: Terminal | F5: Run", VERSION);

    needs_redraw = 1;
    while (1) {
        if (needs_redraw) draw_all();
        int ch = plat_getch();
        if (ch == 27 && !input_mode && mode == MODE_EDITOR) break;
        process_input(ch);
        needs_redraw = 1;
    }

    for (int i = 0; i < tab_count; i++) editor_free(&tabs[i].buf);
    plat_cleanup();
    return 0;
}
