#ifndef HIGHLIGHT_H
#define HIGHLIGHT_H
#include "colors.h"

void highlight_init(void);
Color highlight_get_color(const char *word);
Color highlight_get_char_color(char c);

#endif
