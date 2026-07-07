#ifndef EDITOR_H
#define EDITOR_H

#define MAX_LINES 8192
#define MAX_LINE_LEN 2048
#define MAX_UNDO 256

typedef struct {
    char *lines[MAX_LINES];
    int line_count;
    int cursor_x;
    int cursor_y;
    int scroll_y;
    int scroll_x;
    int modified;
    char filepath[512];
    char name[256];
} EditorBuffer;

typedef struct {
    char *lines[MAX_LINES];
    int count;
} UndoEntry;

void editor_init(EditorBuffer *eb);
void editor_free(EditorBuffer *eb);
int  editor_load(EditorBuffer *eb, const char *path);
int  editor_save(EditorBuffer *eb);
void editor_insert_char(EditorBuffer *eb, char c);
void editor_delete_char(EditorBuffer *eb);
void editor_delete_forward(EditorBuffer *eb);
void editor_newline(EditorBuffer *eb);
void editor_tab(EditorBuffer *eb);
void editor_backspace(EditorBuffer *eb);
void editor_select_all(EditorBuffer *eb);
void editor_delete_line(EditorBuffer *eb);
void editor_duplicate_line(EditorBuffer *eb);
void editor_move_line_up(EditorBuffer *eb);
void editor_move_line_down(EditorBuffer *eb);
void editor_toggle_comment(EditorBuffer *eb);
void editor_goto_line(EditorBuffer *eb, int line);
void editor_set_file(EditorBuffer *eb, const char *path);

#endif
