#include "../core/app.h"
#include "../platform/platform.h"
#include "../editor/editor.h"
#include "hub.h"
#include "project.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void detect_project_lang(const char *path, char *lang_out, int lang_out_size) {
    char check[MAX_PATH_LEN];

    snprintf(check, sizeof(check), "%s/Cargo.toml", path);
    if (plat_file_exists(check)) { strncpy(lang_out, "Rust", lang_out_size); return; }

    snprintf(check, sizeof(check), "%s/build.zig", path);
    if (plat_file_exists(check)) { strncpy(lang_out, "Zig", lang_out_size); return; }

    snprintf(check, sizeof(check), "%s/CMakeLists.txt", path);
    if (plat_file_exists(check)) { strncpy(lang_out, "C++", lang_out_size); return; }

    snprintf(check, sizeof(check), "%s/Makefile", path);
    if (plat_file_exists(check)) {
        snprintf(check, sizeof(check), "%s/main.c", path);
        if (plat_file_exists(check)) { strncpy(lang_out, "C", lang_out_size); return; }
    }

    snprintf(check, sizeof(check), "%s/main.c", path);
    if (plat_file_exists(check)) { strncpy(lang_out, "C", lang_out_size); return; }

    snprintf(check, sizeof(check), "%s/main.cpp", path);
    if (plat_file_exists(check)) { strncpy(lang_out, "C++", lang_out_size); return; }

    snprintf(check, sizeof(check), "%s/src/main.rs", path);
    if (plat_file_exists(check)) { strncpy(lang_out, "Rust", lang_out_size); return; }

    snprintf(check, sizeof(check), "%s/src/main.zig", path);
    if (plat_file_exists(check)) { strncpy(lang_out, "Zig", lang_out_size); return; }

    snprintf(check, sizeof(check), "%s/src/Main.java", path);
    if (plat_file_exists(check)) { strncpy(lang_out, "Java", lang_out_size); return; }

    strncpy(lang_out, "Unknown", lang_out_size);
}

void hub_refresh(void) {
    project_count = 0;
    project_selected = 0;
    project_scroll = 0;

    char cmd[MAX_PATH_LEN * 2];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "dir /b /ad \"%s\"", work_dir);
#else
    snprintf(cmd, sizeof(cmd), "ls -1d \"%s\"/*/ 2>/dev/null | xargs -I{} basename {}", work_dir);
#endif

    FILE *fp = popen(cmd, "r");
    if (!fp) return;

    char buf[MAX_PATH_LEN];
    while (fgets(buf, sizeof(buf), fp) && project_count < MAX_PROJECTS) {
        int len = strlen(buf);
        while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r' || buf[len-1] == '/' || buf[len-1] == '\\'))
            buf[--len] = '\0';
        if (len == 0) continue;

        if (buf[0] == '.') continue;
        if (strcmp(buf, "tools") == 0) continue;

        char fullpath[MAX_PATH_LEN];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", work_dir, buf);

        if (!plat_is_dir(fullpath)) continue;

        char lang[32] = "";
        detect_project_lang(fullpath, lang, sizeof(lang));

        strncpy(projects[project_count].name, buf, MAX_PATH_LEN - 1);
        strncpy(projects[project_count].path, fullpath, MAX_PATH_LEN - 1);
        strncpy(projects[project_count].lang, lang, sizeof(projects[project_count].lang) - 1);
        project_count++;
    }
    pclose(fp);
}

void hub_open_project(void) {
    if (project_count == 0) return;

    char *path = projects[project_selected].path;

    char main_file[MAX_PATH_LEN];

    snprintf(main_file, sizeof(main_file), "%s/main.c", path);
    if (plat_file_exists(main_file)) goto found;

    snprintf(main_file, sizeof(main_file), "%s/main.cpp", path);
    if (plat_file_exists(main_file)) goto found;

    snprintf(main_file, sizeof(main_file), "%s/src/main.rs", path);
    if (plat_file_exists(main_file)) goto found;

    snprintf(main_file, sizeof(main_file), "%s/src/main.zig", path);
    if (plat_file_exists(main_file)) goto found;

    snprintf(main_file, sizeof(main_file), "%s/src/Main.java", path);
    if (plat_file_exists(main_file)) goto found;

    snprintf(main_file, sizeof(main_file), "%s/index.js", path);
    if (plat_file_exists(main_file)) goto found;

    snprintf(main_file, sizeof(main_file), "%s/app.c", path);
    if (plat_file_exists(main_file)) goto found;

    snprintf(main_file, sizeof(main_file), "%s/src/app.c", path);
    if (plat_file_exists(main_file)) goto found;

    snprintf(status_msg, sizeof(status_msg), "No source files found in %s", projects[project_selected].name);
    needs_redraw = 1;
    return;

found:
    if (tab_count < MAX_TABS) {
        for (int i = 0; i < tab_count; i++) {
            if (strcmp(tabs[i].buf.filepath, main_file) == 0) {
                active_tab = i;
                mode = MODE_EDITOR;
                needs_redraw = 1;
                return;
            }
        }
        editor_init(&tabs[tab_count].buf);
        if (editor_load(&tabs[tab_count].buf, main_file) == 0) {
            active_tab = tab_count;
            tab_count++;
        }
    }
    mode = MODE_EDITOR;
    needs_redraw = 1;
}

void hub_delete_project(void) {
    if (project_count == 0) return;

    char cmd[MAX_PATH_LEN * 2];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "rmdir /s /q \"%s\"", projects[project_selected].path);
#else
    snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", projects[project_selected].path);
#endif
    system(cmd);

    snprintf(status_msg, sizeof(status_msg), "Deleted: %s", projects[project_selected].name);
    hub_refresh();
    needs_redraw = 1;
}

void hub_init(void) {
    hub_refresh();
    hub_delete_confirm = 0;
}

void hub_draw(void) {
    plat_get_size();
    plat_cursor_visible(0);

    plat_goto(0, 0);
    set_color_fgbg(CLR_BLACK, CLR_WHITE);
    for (int i = 0; i < con_w; i++) plat_putchar(' ');
    const char *title = " FrogCode - Projects ";
    int tlen = strlen(title);
    plat_goto((con_w - tlen) / 2, 0);
    plat_print(title);
    reset_color();

    int start_y = 2;

    if (project_count == 0) {
        plat_goto(4, start_y + 2);
        set_color(CLR_YELLOW);
        plat_print("No projects found.");
        reset_color();
        plat_goto(4, start_y + 4);
        set_color(CLR_WHITE);
        plat_print("Press 'n' to create your first project.");
        reset_color();
    } else {
        plat_goto(4, start_y);
        set_color(CLR_CYAN);
        plat_printf("Projects (%d):", project_count);
        reset_color();
        start_y += 2;

        int visible = con_h - start_y - 4;
        if (visible < 1) visible = 1;

        for (int i = 0; i < visible && (i + project_scroll) < project_count; i++) {
            int idx = i + project_scroll;
            plat_goto(4, start_y + i);

            if (idx == project_selected) {
                set_color_fgbg(CLR_BLACK, CLR_GREEN);
            } else {
                reset_color();
            }

            char line[MAX_PATH_LEN + 64];
            snprintf(line, sizeof(line), "  %-30s  [%s]", projects[idx].name, projects[idx].lang);
            plat_print(line);
            reset_color();
        }
    }

    int bar_y = con_h - 1;
    plat_goto(0, bar_y);
    set_color_fgbg(CLR_BLACK, CLR_WHITE);
    if (hub_delete_confirm) {
        plat_printf(" Delete '%s'? [y] Yes  [n] No                                  ", projects[project_selected].name);
    } else {
        plat_print(" [Enter] Open  [n] New  [d] Delete  [Esc] Editor                ");
    }
    reset_color();

    if (project_count > 0) {
        int cursor_y = 4 + (project_selected - project_scroll);
        plat_cursor_visible(1);
        plat_goto(2, cursor_y);
    } else {
        plat_cursor_visible(0);
    }

    plat_refresh();
    needs_redraw = 0;
}

void hub_input(int ch) {
    if (hub_delete_confirm) {
        if (ch == 'y' || ch == 'Y') {
            hub_delete_project();
            hub_delete_confirm = 0;
        } else if (ch == 'n' || ch == 'N' || ch == 27) {
            hub_delete_confirm = 0;
        }
        needs_redraw = 1;
        return;
    }

    if (project_count > 0) {
        int visible = con_h - 6;
        if (visible < 1) visible = 1;

        switch (ch) {
            case KEY_UP: case 'k':
                if (project_selected > 0) project_selected--;
                break;
            case KEY_DOWN: case 'j':
                if (project_selected < project_count - 1) project_selected++;
                break;
#ifdef _WIN32
            case 13:
#else
            case KEY_ENTER:
#endif
                hub_open_project();
                return;
            case 'd':
                hub_delete_confirm = 1;
                break;
        }

        if (project_selected < project_scroll) project_scroll = project_selected;
        if (project_selected >= project_scroll + visible) project_scroll = project_selected - visible + 1;
    }

    switch (ch) {
        case 'n':
            project_init();
            mode = MODE_PROJECT;
            break;
        case 27:
            mode = MODE_EDITOR;
            break;
    }
    needs_redraw = 1;
}
