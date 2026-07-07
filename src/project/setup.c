#include "../core/app.h"
#include "../platform/platform.h"
#include "setup.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int install_idx = 0;

static const char *config_path(void) {
    static char path[MAX_PATH_LEN];
#ifdef _WIN32
    snprintf(path, sizeof(path), "%s\\.frogcode_setup", work_dir);
#else
    snprintf(path, sizeof(path), "%s/.frogcode_setup", work_dir);
#endif
    return path;
}

int is_first_run(void) {
    return !plat_file_exists(config_path());
}

void mark_setup_done(void) {
    FILE *f = fopen(config_path(), "w");
    if (f) {
        for (int i = 0; i < lang_count; i++) {
            if (languages[i].installed)
                fprintf(f, "%s\n", languages[i].name);
        }
        fclose(f);
    }
}

static void check_installed(void) {
    for (int i = 0; i < lang_count; i++) {
        languages[i].installed = 0;
        char check_path[MAX_PATH_LEN];
        snprintf(check_path, sizeof(check_path), "%s/%s/%s",
                 work_dir, languages[i].install_dir, languages[i].exe_name);
        if (plat_file_exists(check_path))
            languages[i].installed = 1;
    }
}

void setup_init(void) {
    lang_count = 4;

#ifdef _WIN32
    languages[0].name = "C/C++ (clangd)";
    languages[0].archive_name = "clangd.zip";
    languages[0].url = "https://github.com/clangd/clangd/releases/download/22.1.6/clangd-windows-22.1.6.zip";
    languages[0].install_dir = "tools/clangd";
    languages[0].exe_name = "clangd_22.1.6/bin/clangd.exe";
    languages[0].size_hint = "~27 MB";

    languages[1].name = "Zig";
    languages[1].archive_name = "zig.zip";
    languages[1].url = "https://ziglang.org/download/0.16.0/zig-x86_64-windows-0.16.0.zip";
    languages[1].install_dir = "tools/zig";
    languages[1].exe_name = "zig.exe";
    languages[1].size_hint = "~93 MB";

    languages[2].name = "Rust";
    languages[2].archive_name = "rustup-init.exe";
    languages[2].url = "https://static.rust-lang.org/rustup/dist/x86_64-pc-windows-msvc/rustup-init.exe";
    languages[2].install_dir = "tools/rust";
    languages[2].exe_name = "rustup-init.exe";
    languages[2].size_hint = "~7 MB";

    languages[3].name = "Java (JDK 21)";
    languages[3].archive_name = "java.zip";
    languages[3].url = "https://github.com/adoptium/temurin21-binaries/releases/download/jdk-21.0.11%2B10/OpenJDK21U-jdk_x64_windows_hotspot_21.0.11_10.zip";
    languages[3].install_dir = "tools/java";
    languages[3].exe_name = "bin/java.exe";
    languages[3].size_hint = "~196 MB";
#else
    languages[0].name = "C/C++ (clangd)";
    languages[0].archive_name = "clangd.zip";
    languages[0].url = "https://github.com/clangd/clangd/releases/download/22.1.6/clangd-linux-22.1.6.zip";
    languages[0].install_dir = "tools/clangd";
    languages[0].exe_name = "clangd_22.1.6/bin/clangd";
    languages[0].size_hint = "~27 MB";

    languages[1].name = "Zig";
    languages[1].archive_name = "zig.tar.xz";
    languages[1].url = "https://ziglang.org/download/0.16.0/zig-x86_64-linux-0.16.0.tar.xz";
    languages[1].install_dir = "tools/zig";
    languages[1].exe_name = "zig";
    languages[1].size_hint = "~93 MB";

    languages[2].name = "Rust";
    languages[2].archive_name = "rustup-init";
    languages[2].url = "https://static.rust-lang.org/rustup/dist/x86_64-unknown-linux-gnu/rustup-init";
    languages[2].install_dir = "tools/rust";
    languages[2].exe_name = "rustup-init";
    languages[2].size_hint = "~7 MB";

    languages[3].name = "Java (JDK 21)";
    languages[3].archive_name = "java.tar.gz";
    languages[3].url = "https://github.com/adoptium/temurin21-binaries/releases/download/jdk-21.0.11%2B10/OpenJDK21U-jdk_x64_linux_hotspot_21.0.11_10.tar.gz";
    languages[3].install_dir = "tools/java";
    languages[3].exe_name = "bin/java";
    languages[3].size_hint = "~196 MB";
#endif

    languages[0].selected = 1;
    languages[1].selected = 0;
    languages[2].selected = 0;
    languages[3].selected = 0;

    lang_selected = 0;
    setup_phase = 0;
    setup_status[0] = '\0';
    setup_downloading = 0;
    setup_done = 0;
    install_idx = 0;

    check_installed();
}

void setup_toggle_current(void) {
    if (lang_selected >= 0 && lang_selected < lang_count) {
        if (!languages[lang_selected].installed)
            languages[lang_selected].selected = !languages[lang_selected].selected;
    }
}

static void run_download_command(const char *url, const char *out_path) {
    char cmd[2048];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd),
        "powershell -Command \"[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; (New-Object Net.WebClient).DownloadFile('%s', '%s')\"",
        url, out_path);
#else
    snprintf(cmd, sizeof(cmd), "curl -L -o \"%s\" \"%s\"", out_path, url);
#endif
    snprintf(setup_status, sizeof(setup_status), "Downloading %s ...", languages[install_idx].name);
    needs_redraw = 1;
    system(cmd);
}

static void run_extract_command(const char *archive_path, const char *dest_dir) {
    char cmd[2048];
#ifdef _WIN32
    if (strstr(archive_path, ".zip")) {
        snprintf(cmd, sizeof(cmd),
            "powershell -Command \"if (!(Test-Path '%s')) { New-Item -ItemType Directory -Force -Path '%s' }; Expand-Archive -Path '%s' -DestinationPath '%s' -Force\"",
            dest_dir, dest_dir, archive_path, dest_dir);
    }
#else
    if (strstr(archive_path, ".tar.xz")) {
        snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\" && tar -xJf \"%s\" -C \"%s\" --strip-components=1",
                 dest_dir, archive_path, dest_dir);
    } else if (strstr(archive_path, ".tar.gz")) {
        snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\" && tar -xzf \"%s\" -C \"%s\" --strip-components=1",
                 dest_dir, archive_path, dest_dir);
    } else if (strstr(archive_path, ".zip")) {
        snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\" && unzip -o \"%s\" -d \"%s\"",
                 dest_dir, archive_path, dest_dir);
    }
#endif
    snprintf(setup_status, sizeof(setup_status), "Extracting %s ...", languages[install_idx].name);
    needs_redraw = 1;
    system(cmd);
}

static void install_language(int idx) {
    Language *lang = &languages[idx];
    if (lang->installed) return;

    char tools_dir[MAX_PATH_LEN];
    snprintf(tools_dir, sizeof(tools_dir), "%s/tools", work_dir);
    plat_mkdir(tools_dir);

    char lang_dir[MAX_PATH_LEN];
    snprintf(lang_dir, sizeof(lang_dir), "%s/%s", work_dir, lang->install_dir);

#ifdef _WIN32
    if (strstr(lang->url, ".exe")) {
        char exe_path[MAX_PATH_LEN];
        snprintf(exe_path, sizeof(exe_path), "%s/%s", lang_dir, lang->exe_name);
        plat_mkdir(lang_dir);
        run_download_command(lang->url, exe_path);
    } else
#endif
    {
        char archive_path[MAX_PATH_LEN];
        snprintf(archive_path, sizeof(archive_path), "%s/%s", tools_dir, lang->archive_name);
        run_download_command(lang->url, archive_path);
        plat_mkdir(lang_dir);
        run_extract_command(archive_path, lang_dir);
        remove(archive_path);
    }

    lang->installed = 1;
    snprintf(setup_status, sizeof(setup_status), "%s installed!", lang->name);
    needs_redraw = 1;
}

void setup_start_install(void) {
    setup_phase = 1;
    install_idx = 0;
    setup_install_next();
}

void setup_install_next(void) {
    while (install_idx < lang_count) {
        if (languages[install_idx].selected && !languages[install_idx].installed) {
            install_language(install_idx);
            install_idx++;
            return;
        }
        install_idx++;
    }
    setup_phase = 2;
    setup_done = 1;
    mark_setup_done();
    snprintf(setup_status, sizeof(setup_status), "Setup complete! Press Enter to continue.");
    needs_redraw = 1;
}

void setup_draw(void) {
    plat_get_size();
    plat_cursor_visible(0);

    int start_y = 2;

    plat_goto(0, 0);
    set_color_fgbg(CLR_BLACK, CLR_WHITE);
    for (int i = 0; i < con_w; i++) plat_putchar(' ');
    const char *title = " FrogCode - First Run Setup ";
    int tlen = strlen(title);
    plat_goto((con_w - tlen) / 2, 0);
    plat_print(title);
    reset_color();

    plat_goto(0, start_y);
    set_color(CLR_CYAN);
    plat_printf("  Welcome to FrogCode! Select languages to install:");
    reset_color();
    start_y += 2;

    for (int i = 0; i < lang_count; i++) {
        Language *lang = &languages[i];
        plat_goto(4, start_y + i);

        if (i == lang_selected) {
            set_color_fgbg(CLR_BLACK, CLR_GREEN);
        } else {
            reset_color();
        }

        const char *checkbox = lang->installed ? "[v] " :
                               (lang->selected ? "[x] " : "[ ] ");
        char line[256];
        snprintf(line, sizeof(line), "%s%-20s %s", checkbox, lang->name, lang->size_hint);
        plat_print(line);

        if (lang->installed) {
            set_color(CLR_GREEN);
            plat_print("  (installed)");
        }

        reset_color();
    }

    int info_y = start_y + lang_count + 2;
    plat_goto(4, info_y);
    set_color(CLR_WHITE);
    if (languages[lang_selected].installed) {
        plat_print("This language is already installed.");
    } else if (languages[lang_selected].selected) {
        plat_print("Will be installed when you press Enter.");
    } else {
        plat_print("Press Space to toggle selection.");
    }
    reset_color();

    if (setup_phase == 1 && setup_status[0]) {
        plat_goto(4, info_y + 2);
        set_color(CLR_YELLOW);
        plat_print(setup_status);
        reset_color();

        plat_goto(4, info_y + 3);
        set_color(CLR_WHITE);
        for (int i = 0; i < 40; i++) plat_putchar('-');
        reset_color();
    }

    if (setup_done) {
        plat_goto(4, info_y + 2);
        set_color(CLR_GREEN);
        plat_print(setup_status);
        reset_color();
    }

    int bar_y = con_h - 1;
    plat_goto(0, bar_y);
    set_color_fgbg(CLR_BLACK, CLR_WHITE);
    if (setup_phase == 0) {
        plat_print(" [Space] Toggle  [Enter] Install  [Esc] Skip                  ");
    } else if (setup_phase == 1) {
        plat_print(" Installing... please wait                                    ");
    } else {
        plat_print(" [Enter] Continue                                            ");
    }
    reset_color();

    if (setup_phase == 0) {
        int cursor_y = start_y + lang_selected;
        int cursor_x = 2;
        plat_cursor_visible(1);
        plat_goto(cursor_x, cursor_y);
    } else {
        plat_cursor_visible(0);
    }

    plat_refresh();
    needs_redraw = 0;
}

void setup_input(int ch) {
    if (setup_phase == 0) {
        switch (ch) {
            case KEY_UP: case 'k':
                if (lang_selected > 0) lang_selected--;
                break;
            case KEY_DOWN: case 'j':
                if (lang_selected < lang_count - 1) lang_selected++;
                break;
            case ' ':
                setup_toggle_current();
                break;
#ifdef _WIN32
            case 13:
#else
            case KEY_ENTER:
#endif
                setup_start_install();
                break;
            case 27:
                mark_setup_done();
                mode = MODE_EDITOR;
                break;
        }
    } else if (setup_phase == 1) {
        setup_install_next();
    } else if (setup_phase == 2) {
        if (ch == 13 || ch == KEY_ENTER) {
            mode = MODE_EDITOR;
        }
    }
    needs_redraw = 1;
}
