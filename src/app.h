#ifndef APP_H
#define APP_H

#include "editor.h"
#include "colors.h"

#define VERSION "1.0"
#define MAX_TABS 16
#define MAX_PATH_LEN 512
#define MAX_SEARCH_LEN 256
#define SIDEBAR_WIDTH 28

typedef enum { MODE_EDITOR, MODE_FILETREE, MODE_TERMINAL, MODE_SEARCH, MODE_COMMAND } AppMode;
typedef enum { PANEL_FILES, PANEL_SEARCH } SidebarPanel;

typedef struct { EditorBuffer buf; } Tab;

extern int con_w, con_h;
extern AppMode mode;
extern SidebarPanel sidebar_panel;
extern int sidebar_open;
extern int terminal_open;

extern Tab tabs[MAX_TABS];
extern int tab_count;
extern int active_tab;

extern char search_query[MAX_SEARCH_LEN];
extern int search_active;

extern char terminal_cmd[4096];
extern int terminal_cmd_len;
extern char terminal_out[16384];
extern int terminal_out_len;

extern char work_dir[MAX_PATH_LEN];
extern char file_tree_entries[512][MAX_PATH_LEN];
extern char file_tree_names[512][256];
extern int file_tree_count;
extern int file_tree_selected;
extern int file_tree_scroll;

extern char status_msg[256];
extern char input_buf[512];
extern int input_len;
extern int input_mode;
extern char input_prompt[128];

extern int needs_redraw;

EditorBuffer *cur_buf(void);

#endif
