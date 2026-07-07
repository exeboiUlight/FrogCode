#include "../core/app.h"
#include "../platform/platform.h"
#include "project.h"
#include "../editor/editor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    if (f) {
        fputs(content, f);
        fclose(f);
    }
}

void project_init(void) {
    template_count = 5;

    templates[0].name = "C (main.c + Makefile)";
    templates[0].lang = "c";
    templates[0].dir_name = "c_project";
    templates[0].files[0] = "main.c";
    templates[0].contents[0] =
        "#include <stdio.h>\n"
        "\n"
        "int main(void) {\n"
        "    printf(\"Hello, World!\\n\");\n"
        "    return 0;\n"
        "}\n";
    templates[0].files[1] = "Makefile";
    templates[0].contents[1] =
        "CC = gcc\n"
        "CFLAGS = -Wall -Wextra -O2\n"
        "\n"
        "all: main\n"
        "\n"
        "main: main.c\n"
        "\t$(CC) $(CFLAGS) -o main main.c\n"
        "\n"
        "clean:\n"
        "\trm -f main\n"
        "\n"
        ".PHONY: all clean\n";
    templates[0].file_count = 2;

    templates[1].name = "C++ (main.cpp + CMakeLists.txt)";
    templates[1].lang = "cpp";
    templates[1].dir_name = "cpp_project";
    templates[1].files[0] = "main.cpp";
    templates[1].contents[0] =
        "#include <iostream>\n"
        "\n"
        "int main() {\n"
        "    std::cout << \"Hello, World!\" << std::endl;\n"
        "    return 0;\n"
        "}\n";
    templates[1].files[1] = "CMakeLists.txt";
    templates[1].contents[1] =
        "cmake_minimum_required(VERSION 3.10)\n"
        "project(MyProject LANGUAGES CXX)\n"
        "\n"
        "set(CMAKE_CXX_STANDARD 17)\n"
        "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n"
        "\n"
        "add_executable(main main.cpp)\n";
    templates[1].file_count = 2;

    templates[2].name = "Zig (build.zig + src/main.zig)";
    templates[2].lang = "zig";
    templates[2].dir_name = "zig_project";
    templates[2].files[0] = "build.zig";
    templates[2].contents[0] =
        "const std = @import(\"std\");\n"
        "\n"
        "pub fn build(b: *std.Build) void {\n"
        "    const target = b.standardTargetOptions(.{});\n"
        "    const optimize = b.standardOptimizeOption(.{});\n"
        "\n"
        "    const exe = b.addExecutable(.{\n"
        "        .name = \"main\",\n"
        "        .root_source_file = b.path(\"src/main.zig\"),\n"
        "        .target = target,\n"
        "        .optimize = optimize,\n"
        "    });\n"
        "\n"
        "    b.installArtifact(exe);\n"
        "\n"
        "    const run_cmd = b.addRunArtifact(exe);\n"
        "    run_cmd.step.dependOn(b.getInstallStep());\n"
        "\n"
        "    const run_step = b.step(\"run\", \"Run the app\");\n"
        "    run_step.dependOn(&run_cmd.step);\n"
        "}\n";
    templates[2].files[1] = "src/main.zig";
    templates[2].contents[1] =
        "const std = @import(\"std\");\n"
        "\n"
        "pub fn main() void {\n"
        "    std.debug.print(\"Hello, World!\\n\", .{});\n"
        "}\n";
    templates[2].file_count = 2;

    templates[3].name = "Rust (Cargo.toml + src/main.rs)";
    templates[3].lang = "rust";
    templates[3].dir_name = "rust_project";
    templates[3].files[0] = "Cargo.toml";
    templates[3].contents[0] =
        "[package]\n"
        "name = \"rust_project\"\n"
        "version = \"0.1.0\"\n"
        "edition = \"2021\"\n"
        "\n"
        "[dependencies]\n";
    templates[3].files[1] = "src/main.rs";
    templates[3].contents[1] =
        "fn main() {\n"
        "    println!(\"Hello, World!\");\n"
        "}\n";
    templates[3].file_count = 2;

    templates[4].name = "Java (src/Main.java)";
    templates[4].lang = "java";
    templates[4].dir_name = "java_project";
    templates[4].files[0] = "src/Main.java";
    templates[4].contents[0] =
        "public class Main {\n"
        "    public static void main(String[] args) {\n"
        "        System.out.println(\"Hello, World!\");\n"
        "    }\n"
        "}\n";
    templates[4].file_count = 1;

    template_selected = 0;
    project_name_len = 0;
    project_name[0] = '\0';
    project_name_input = 1;
}

void project_create(void) {
    if (project_name_len == 0) return;

    char proj_dir[MAX_PATH_LEN];
    snprintf(proj_dir, sizeof(proj_dir), "%s/%s", work_dir, project_name);
    plat_mkdir(proj_dir);

    ProjectTemplate *t = &templates[template_selected];

    if (strcmp(t->lang, "zig") == 0 || strcmp(t->lang, "rust") == 0 ||
        strcmp(t->lang, "java") == 0) {
        char subdir[MAX_PATH_LEN];
        snprintf(subdir, sizeof(subdir), "%s/src", proj_dir);
        plat_mkdir(subdir);
    }

    for (int i = 0; i < t->file_count; i++) {
        char filepath[MAX_PATH_LEN];
        snprintf(filepath, sizeof(filepath), "%s/%s", proj_dir, t->files[i]);

        const char *slash = strrchr(t->files[i], '/');
        if (slash) {
            char dirpart[MAX_PATH_LEN];
            int len = slash - t->files[i];
            memcpy(dirpart, t->files[i], len);
            dirpart[len] = '\0';
            char fulldir[MAX_PATH_LEN];
            snprintf(fulldir, sizeof(fulldir), "%s/%s", proj_dir, dirpart);
            plat_mkdir(fulldir);
        }

        write_file(filepath, t->contents[i]);
    }

    snprintf(status_msg, sizeof(status_msg), "Project '%s' created!", project_name);

    if (tab_count < MAX_TABS) {
        char main_file[MAX_PATH_LEN];
        snprintf(main_file, sizeof(main_file), "%s/%s", proj_dir, t->files[0]);
        editor_init(&tabs[tab_count].buf);
        if (editor_load(&tabs[tab_count].buf, main_file) == 0) {
            active_tab = tab_count;
            tab_count++;
        }
    }

    mode = MODE_EDITOR;
    needs_redraw = 1;
}

void project_draw(void) {
    plat_get_size();
    plat_cursor_visible(0);

    int start_y = 2;

    plat_goto(0, 0);
    set_color_fgbg(CLR_BLACK, CLR_WHITE);
    for (int i = 0; i < con_w; i++) plat_putchar(' ');
    const char *title = " Create New Project ";
    int tlen = strlen(title);
    plat_goto((con_w - tlen) / 2, 0);
    plat_print(title);
    reset_color();

    plat_goto(4, start_y);
    set_color(CLR_CYAN);
    plat_print("Project name:");
    reset_color();

    plat_goto(4, start_y + 1);
    set_color_fgbg(CLR_WHITE, CLR_BLACK);
    char name_display[64];
    snprintf(name_display, sizeof(name_display), "> %s_", project_name);
    plat_print(name_display);
    reset_color();

    plat_goto(4, start_y + 3);
    set_color(CLR_CYAN);
    plat_print("Language / Template:");
    reset_color();

    for (int i = 0; i < template_count; i++) {
        plat_goto(6, start_y + 5 + i);
        if (i == template_selected) {
            set_color_fgbg(CLR_BLACK, CLR_GREEN);
        } else {
            reset_color();
        }
        char line[128];
        snprintf(line, sizeof(line), "  %s  ", templates[i].name);
        plat_print(line);
        reset_color();
    }

    int info_y = start_y + 5 + template_count + 1;
    plat_goto(4, info_y);
    set_color(CLR_WHITE);
    ProjectTemplate *t = &templates[template_selected];
    char info[256];
    snprintf(info, sizeof(info), "Will create: %s/", project_name[0] ? project_name : "...");
    plat_print(info);

    for (int i = 0; i < t->file_count; i++) {
        plat_goto(6, info_y + 1 + i);
        set_color(CLR_GREEN);
        char fline[128];
        snprintf(fline, sizeof(fline), "+ %s", t->files[i]);
        plat_print(fline);
        reset_color();
    }

    int bar_y = con_h - 1;
    plat_goto(0, bar_y);
    set_color_fgbg(CLR_BLACK, CLR_WHITE);
    if (project_name_input) {
        plat_print(" Type project name  [Tab] Switch to language  [Enter] Create  [Esc] Cancel ");
    } else {
        plat_print(" [Up/Down] Select  [Tab] Switch to name  [Enter] Create  [Esc] Cancel     ");
    }
    reset_color();

    if (project_name_input) {
        plat_cursor_visible(1);
        plat_goto(6 + project_name_len, start_y + 1);
    } else {
        plat_cursor_visible(1);
        int cursor_y = start_y + 5 + template_selected;
        plat_goto(4, cursor_y);
    }

    plat_refresh();
    needs_redraw = 0;
}

void project_input(int ch) {
    if (project_name_input) {
        if (ch == 13 && project_name_len > 0) {
            project_create();
            return;
        }
        if (ch == 27) {
            mode = MODE_EDITOR;
            needs_redraw = 1;
            return;
        }
        if (ch == 9) {
            project_name_input = 0;
            needs_redraw = 1;
            return;
        }
        if (ch == 8) {
            if (project_name_len > 0) {
                project_name[--project_name_len] = '\0';
            }
            needs_redraw = 1;
            return;
        }
        if ((isalnum(ch) || ch == '_' || ch == '-') && project_name_len < (int)sizeof(project_name) - 1) {
            project_name[project_name_len++] = ch;
            project_name[project_name_len] = '\0';
        }
    } else {
        if (ch == 27) {
            mode = MODE_EDITOR;
            needs_redraw = 1;
            return;
        }
        if (ch == 9) {
            project_name_input = 1;
            needs_redraw = 1;
            return;
        }
        switch (ch) {
            case KEY_UP: case 'k':
                if (template_selected > 0) template_selected--;
                break;
            case KEY_DOWN: case 'j':
                if (template_selected < template_count - 1) template_selected++;
                break;
#ifdef _WIN32
            case 13:
#else
            case KEY_ENTER:
#endif
                if (project_name_len > 0) project_create();
                break;
        }
    }
    needs_redraw = 1;
}
