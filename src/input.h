#ifndef INPUT_H
#define INPUT_H

void start_input(const char *prompt);
void handle_input_key(int ch);
void open_file_dialog(void);
void save_file_dialog(void);
void goto_line_dialog(void);
void new_file(void);
void close_tab(void);
void select_all(void);
void process_input(int ch);

#endif
