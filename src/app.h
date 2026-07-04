#ifndef APP_H
#define APP_H

#include "editor.h"
#include "colors.h"

#define VERSION "2.0"
#define MAX_TABS 16
#define MAX_PATH_LEN 512
#define MAX_SEARCH_LEN 256
#define SIDEBAR_WIDTH 28
#define MAX_LANGUAGES 4
#define MAX_TEMPLATES 5
#define MAX_PROJECTS 128

typedef enum {
    MODE_EDITOR, MODE_FILETREE, MODE_TERMINAL, MODE_SEARCH, MODE_COMMAND,
    MODE_SETUP, MODE_PROJECT, MODE_HUB
} AppMode;
typedef enum { PANEL_FILES, PANEL_SEARCH } SidebarPanel;

typedef struct { EditorBuffer buf; } Tab;

typedef struct {
    const char *name;
    const char *archive_name;
    const char *url;
    const char *install_dir;
    const char *exe_name;
    const char *size_hint;
    int selected;
    int installed;
} Language;

typedef struct {
    const char *name;
    const char *lang;
    const char *dir_name;
    const char *files[8];
    const char *contents[8];
    int file_count;
} ProjectTemplate;

typedef struct {
    char name[MAX_PATH_LEN];
    char path[MAX_PATH_LEN];
    char lang[32];
} ProjectEntry;

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

extern Language languages[MAX_LANGUAGES];
extern int lang_count;
extern int lang_selected;
extern int setup_phase;
extern char setup_status[256];
extern int setup_downloading;
extern int setup_done;

extern ProjectTemplate templates[MAX_TEMPLATES];
extern int template_count;
extern int template_selected;
extern char project_name[256];
extern int project_name_len;
extern int project_name_input;
extern int project_lang_selected;

extern ProjectEntry projects[MAX_PROJECTS];
extern int project_count;
extern int project_selected;
extern int project_scroll;
extern int hub_delete_confirm;

EditorBuffer *cur_buf(void);
int is_first_run(void);
void mark_setup_done(void);

#endif
