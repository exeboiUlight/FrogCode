#include "../core/app.h"
#include "../platform/platform.h"
#include "../editor/editor.h"
#include "filetree.h"

#ifdef _WIN32

void scan_directory(const char *dir, int depth) {
    WIN32_FIND_DATAA fd;
    char pattern[MAX_PATH_LEN];
    snprintf(pattern, sizeof(pattern), "%s\\*", dir);
    HANDLE hFind = FindFirstFileA(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) continue;
        if (file_tree_count >= 512) break;
        char fullpath[MAX_PATH_LEN];
        snprintf(fullpath, sizeof(fullpath), "%s\\%s", dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            snprintf(file_tree_names[file_tree_count], 256, "%*s[+] %s", depth * 2, "", fd.cFileName);
        else
            snprintf(file_tree_names[file_tree_count], 256, "%*s    %s", depth * 2, "", fd.cFileName);
        snprintf(file_tree_entries[file_tree_count], MAX_PATH_LEN, "%s", fullpath);
        file_tree_count++;
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && depth < 3)
            scan_directory(fullpath, depth + 1);
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
}

#else

void scan_directory(const char *dir, int depth) {
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        if (ent->d_name[0] == '.') continue;
        if (file_tree_count >= 512) break;
        char fullpath[MAX_PATH_LEN];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, ent->d_name);
        int is_dir = 0;
        struct stat st;
        if (stat(fullpath, &st) == 0) is_dir = S_ISDIR(st.st_mode);
        if (is_dir)
            snprintf(file_tree_names[file_tree_count], 256, "%*s[+] %s", depth * 2, "", ent->d_name);
        else
            snprintf(file_tree_names[file_tree_count], 256, "%*s    %s", depth * 2, "", ent->d_name);
        snprintf(file_tree_entries[file_tree_count], MAX_PATH_LEN, "%s", fullpath);
        file_tree_count++;
        if (is_dir && depth < 3) scan_directory(fullpath, depth + 1);
    }
    closedir(d);
}

#endif

void refresh_file_tree(void) {
    file_tree_count = 0;
    file_tree_selected = 0;
    file_tree_scroll = 0;
    scan_directory(work_dir, 0);
}

void open_file_from_tree(void) {
    if (file_tree_count == 0) return;
    char *path = file_tree_entries[file_tree_selected];
    if (plat_is_dir(path)) return;
    if (tab_count < MAX_TABS) {
        for (int i = 0; i < tab_count; i++) {
            if (strcmp(tabs[i].buf.filepath, path) == 0) {
                active_tab = i;
                mode = MODE_EDITOR;
                needs_redraw = 1;
                return;
            }
        }
        editor_init(&tabs[tab_count].buf);
        if (editor_load(&tabs[tab_count].buf, path) == 0) {
            active_tab = tab_count;
            tab_count++;
        }
        mode = MODE_EDITOR;
    }
    needs_redraw = 1;
}
