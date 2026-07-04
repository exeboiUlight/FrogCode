#include "app.h"
#include "platform.h"
#include "editor.h"
#include "terminal.h"

void terminal_execute(const char *cmd) {
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hRead, hWrite;
    CreatePipe(&hRead, &hWrite, &sa, 0);

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));
    char cmdline[4096];
    snprintf(cmdline, sizeof(cmdline), "cmd /c \"%s\"", cmd);

    int ok = CreateProcessA(NULL, cmdline, NULL, NULL, TRUE,
                            CREATE_NO_WINDOW, NULL, work_dir, &si, &pi);
    CloseHandle(hWrite);

    if (ok) {
        char buf[4096];
        DWORD n;
        terminal_out_len = 0;
        while (ReadFile(hRead, buf, sizeof(buf) - 1, &n, NULL) && n > 0) {
            int copy = (int)n;
            if (terminal_out_len + copy > (int)sizeof(terminal_out) - 1)
                copy = (int)sizeof(terminal_out) - 1 - terminal_out_len;
            memcpy(terminal_out + terminal_out_len, buf, copy);
            terminal_out_len += copy;
            terminal_out[terminal_out_len] = '\0';
        }
        WaitForSingleObject(pi.hProcess, 10000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        snprintf(terminal_out, sizeof(terminal_out), "Failed to start process");
        terminal_out_len = strlen(terminal_out);
    }
    CloseHandle(hRead);
#else
    char full_cmd[4096];
    snprintf(full_cmd, sizeof(full_cmd), "cd \"%s\" && %s 2>&1", work_dir, cmd);
    FILE *fp = popen(full_cmd, "r");
    if (!fp) {
        snprintf(terminal_out, sizeof(terminal_out), "Failed to start process");
        terminal_out_len = strlen(terminal_out);
        return;
    }
    terminal_out_len = 0;
    char buf[4096];
    while (fgets(buf, sizeof(buf), fp)) {
        int len = strlen(buf);
        if (terminal_out_len + len < (int)sizeof(terminal_out) - 1) {
            memcpy(terminal_out + terminal_out_len, buf, len);
            terminal_out_len += len;
            terminal_out[terminal_out_len] = '\0';
        }
    }
    pclose(fp);
#endif
}

void build_and_run(void) {
    EditorBuffer *eb = cur_buf();
    if (!eb || !eb->filepath[0]) {
        snprintf(status_msg, sizeof(status_msg), "Save file first (Ctrl+S)");
        needs_redraw = 1;
        return;
    }
    if (eb->modified) editor_save(eb);

    char exe_path[MAX_PATH_LEN];
    char *dot = strrchr(eb->filepath, '.');
    if (dot) {
        int len = dot - eb->filepath;
        memcpy(exe_path, eb->filepath, len);
        exe_path[len] = '\0';
#ifdef _WIN32
        strcat(exe_path, ".exe");
#else
        strcat(exe_path, ".out");
#endif
    } else {
        snprintf(exe_path, sizeof(exe_path), "%s.out", eb->filepath);
    }

    terminal_open = 1;
    mode = MODE_TERMINAL;

    char build_cmd[2048];
    const char *ext = dot ? dot : "";
    if (strcmp(ext, ".c") == 0)
        snprintf(build_cmd, sizeof(build_cmd), "gcc -Wall -o \"%s\" \"%s\" 2>&1 && echo === OK === && \"%s\"",
                 exe_path, eb->filepath, exe_path);
    else if (strcmp(ext, ".cpp") == 0 || strcmp(ext, ".cc") == 0)
        snprintf(build_cmd, sizeof(build_cmd), "g++ -Wall -o \"%s\" \"%s\" 2>&1 && echo === OK === && \"%s\"",
                 exe_path, eb->filepath, exe_path);
    else if (strcmp(ext, ".py") == 0)
        snprintf(build_cmd, sizeof(build_cmd), "python3 \"%s\"", eb->filepath);
    else {
        snprintf(status_msg, sizeof(status_msg), "Unsupported file type: %s", ext);
        needs_redraw = 1;
        return;
    }
    terminal_execute(build_cmd);
    needs_redraw = 1;
}
