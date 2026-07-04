#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "editor.h"

void editor_init(EditorBuffer *eb) {
    memset(eb, 0, sizeof(*eb));
    eb->line_count = 1;
    eb->lines[0] = malloc(MAX_LINE_LEN);
    eb->lines[0][0] = '\0';
    eb->filepath[0] = '\0';
    strcpy(eb->name, "Untitled");
}

void editor_free(EditorBuffer *eb) {
    for (int i = 0; i < eb->line_count; i++) {
        if (eb->lines[i]) { free(eb->lines[i]); eb->lines[i] = NULL; }
    }
    eb->line_count = 0;
}

static void ensure_line(EditorBuffer *eb, int idx) {
    if (!eb->lines[idx]) {
        eb->lines[idx] = malloc(MAX_LINE_LEN);
        eb->lines[idx][0] = '\0';
    }
}

void editor_set_file(EditorBuffer *eb, const char *path) {
    strncpy(eb->filepath, path, sizeof(eb->filepath) - 1);
    eb->filepath[sizeof(eb->filepath) - 1] = '\0';
    const char *slash = strrchr(path, '/');
    if (!slash) slash = strrchr(path, '\\');
    if (slash) strncpy(eb->name, slash + 1, sizeof(eb->name) - 1);
    else strncpy(eb->name, path, sizeof(eb->name) - 1);
}

int editor_load(EditorBuffer *eb, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    editor_free(eb);
    eb->line_count = 0;
    char buf[MAX_LINE_LEN];
    while (fgets(buf, sizeof(buf), f) && eb->line_count < MAX_LINES) {
        int len = strlen(buf);
        while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r'))
            buf[--len] = '\0';
        ensure_line(eb, eb->line_count);
        strncpy(eb->lines[eb->line_count], buf, MAX_LINE_LEN - 1);
        eb->lines[eb->line_count][MAX_LINE_LEN - 1] = '\0';
        eb->line_count++;
    }
    fclose(f);
    if (eb->line_count == 0) {
        ensure_line(eb, 0);
        eb->line_count = 1;
    }
    eb->cursor_x = 0;
    eb->cursor_y = 0;
    eb->scroll_y = 0;
    eb->scroll_x = 0;
    eb->modified = 0;
    editor_set_file(eb, path);
    return 0;
}

int editor_save(EditorBuffer *eb) {
    if (!eb->filepath[0]) return -1;
    FILE *f = fopen(eb->filepath, "w");
    if (!f) return -1;
    for (int i = 0; i < eb->line_count; i++) {
        fprintf(f, "%s\n", eb->lines[i] ? eb->lines[i] : "");
    }
    fclose(f);
    eb->modified = 0;
    return 0;
}

void editor_insert_char(EditorBuffer *eb, char c) {
    ensure_line(eb, eb->cursor_y);
    int len = strlen(eb->lines[eb->cursor_y]);
    if (len < MAX_LINE_LEN - 1) {
        memmove(&eb->lines[eb->cursor_y][eb->cursor_x + 1],
                &eb->lines[eb->cursor_y][eb->cursor_x],
                len - eb->cursor_x + 1);
        eb->lines[eb->cursor_y][eb->cursor_x] = c;
        eb->cursor_x++;
        eb->modified = 1;
    }
}

void editor_delete_char(EditorBuffer *eb) {
    ensure_line(eb, eb->cursor_y);
    if (eb->cursor_x > 0) {
        int len = strlen(eb->lines[eb->cursor_y]);
        memmove(&eb->lines[eb->cursor_y][eb->cursor_x - 1],
                &eb->lines[eb->cursor_y][eb->cursor_x],
                len - eb->cursor_x + 1);
        eb->cursor_x--;
        eb->modified = 1;
    } else if (eb->cursor_y > 0) {
        int prev_len = strlen(eb->lines[eb->cursor_y - 1]);
        int cur_len = strlen(eb->lines[eb->cursor_y]);
        if (prev_len + cur_len < MAX_LINE_LEN - 1) {
            memcpy(&eb->lines[eb->cursor_y - 1][prev_len],
                   eb->lines[eb->cursor_y], cur_len + 1);
            free(eb->lines[eb->cursor_y]);
            for (int i = eb->cursor_y; i < eb->line_count - 1; i++)
                eb->lines[i] = eb->lines[i + 1];
            eb->lines[eb->line_count - 1] = NULL;
            eb->line_count--;
            eb->cursor_y--;
            eb->cursor_x = prev_len;
            eb->modified = 1;
        }
    }
}

void editor_delete_forward(EditorBuffer *eb) {
    ensure_line(eb, eb->cursor_y);
    int len = strlen(eb->lines[eb->cursor_y]);
    if (eb->cursor_x < len) {
        memmove(&eb->lines[eb->cursor_y][eb->cursor_x],
                &eb->lines[eb->cursor_y][eb->cursor_x + 1],
                len - eb->cursor_x);
        eb->modified = 1;
    } else if (eb->cursor_y < eb->line_count - 1) {
        int cur_len = strlen(eb->lines[eb->cursor_y]);
        int next_len = strlen(eb->lines[eb->cursor_y + 1]);
        if (cur_len + next_len < MAX_LINE_LEN - 1) {
            memcpy(&eb->lines[eb->cursor_y][cur_len],
                   eb->lines[eb->cursor_y + 1], next_len + 1);
            free(eb->lines[eb->cursor_y + 1]);
            for (int i = eb->cursor_y + 1; i < eb->line_count - 1; i++)
                eb->lines[i] = eb->lines[i + 1];
            eb->lines[eb->line_count - 1] = NULL;
            eb->line_count--;
            eb->modified = 1;
        }
    }
}

void editor_newline(EditorBuffer *eb) {
    ensure_line(eb, eb->cursor_y);
    if (eb->line_count >= MAX_LINES) return;
    for (int i = eb->line_count; i > eb->cursor_y + 1; i--)
        eb->lines[i] = eb->lines[i - 1];
    eb->lines[eb->cursor_y + 1] = NULL;
    ensure_line(eb, eb->cursor_y + 1);
    eb->line_count++;
    char *rest = &eb->lines[eb->cursor_y][eb->cursor_x];
    strncpy(eb->lines[eb->cursor_y + 1], rest, MAX_LINE_LEN - 1);
    eb->lines[eb->cursor_y + 1][MAX_LINE_LEN - 1] = '\0';
    rest[0] = '\0';
    eb->cursor_y++;
    int indent = 0;
    char *prev = eb->lines[eb->cursor_y - 1];
    while (prev[indent] == ' ' || prev[indent] == '\t') indent++;
    if (indent > 0 && indent < MAX_LINE_LEN - 1) {
        memmove(&eb->lines[eb->cursor_y][indent], &eb->lines[eb->cursor_y][0],
                strlen(eb->lines[eb->cursor_y]) + 1);
        memcpy(eb->lines[eb->cursor_y], prev, indent);
    }
    eb->cursor_x = indent;
    eb->modified = 1;
}

void editor_tab(EditorBuffer *eb) {
    for (int i = 0; i < 4; i++)
        editor_insert_char(eb, ' ');
}

void editor_backspace(EditorBuffer *eb) {
    editor_delete_char(eb);
}

void editor_delete_line(EditorBuffer *eb) {
    if (eb->line_count <= 1) {
        eb->lines[0][0] = '\0';
        eb->cursor_x = 0;
        eb->modified = 1;
        return;
    }
    free(eb->lines[eb->cursor_y]);
    for (int i = eb->cursor_y; i < eb->line_count - 1; i++)
        eb->lines[i] = eb->lines[i + 1];
    eb->lines[eb->line_count - 1] = NULL;
    eb->line_count--;
    if (eb->cursor_y >= eb->line_count) eb->cursor_y = eb->line_count - 1;
    eb->cursor_x = 0;
    eb->modified = 1;
}

void editor_duplicate_line(EditorBuffer *eb) {
    if (eb->line_count >= MAX_LINES) return;
    ensure_line(eb, eb->cursor_y);
    for (int i = eb->line_count; i > eb->cursor_y; i--)
        eb->lines[i] = eb->lines[i - 1];
    eb->lines[eb->cursor_y] = NULL;
    ensure_line(eb, eb->cursor_y);
    strncpy(eb->lines[eb->cursor_y], eb->lines[eb->cursor_y + 1], MAX_LINE_LEN - 1);
    eb->lines[eb->cursor_y][MAX_LINE_LEN - 1] = '\0';
    eb->line_count++;
    eb->cursor_y++;
    eb->modified = 1;
}

void editor_move_line_up(EditorBuffer *eb) {
    if (eb->cursor_y <= 0) return;
    char *tmp = eb->lines[eb->cursor_y];
    eb->lines[eb->cursor_y] = eb->lines[eb->cursor_y - 1];
    eb->lines[eb->cursor_y - 1] = tmp;
    eb->cursor_y--;
    eb->modified = 1;
}

void editor_move_line_down(EditorBuffer *eb) {
    if (eb->cursor_y >= eb->line_count - 1) return;
    char *tmp = eb->lines[eb->cursor_y];
    eb->lines[eb->cursor_y] = eb->lines[eb->cursor_y + 1];
    eb->lines[eb->cursor_y + 1] = tmp;
    eb->cursor_y++;
    eb->modified = 1;
}

void editor_toggle_comment(EditorBuffer *eb) {
    ensure_line(eb, eb->cursor_y);
    char *line = eb->lines[eb->cursor_y];
    int len = strlen(line);
    int is_commented = (len >= 2 && line[0] == '/' && line[1] == '/');
    if (is_commented) {
        memmove(line, line + 2, len - 1);
        if (eb->cursor_x >= 2) eb->cursor_x -= 2;
        else eb->cursor_x = 0;
    } else {
        if (len + 2 < MAX_LINE_LEN - 1) {
            memmove(line + 2, line, len + 1);
            line[0] = '/';
            line[1] = '/';
            eb->cursor_x += 2;
        }
    }
    eb->modified = 1;
}

void editor_goto_line(EditorBuffer *eb, int line) {
    if (line >= 1 && line <= eb->line_count) {
        eb->cursor_y = line - 1;
        eb->cursor_x = 0;
    }
}
