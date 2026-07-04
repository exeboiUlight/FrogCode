#include "highlight.h"
#include <string.h>
#include <ctype.h>

static const char *keywords_c[] = {
    "int", "char", "void", "float", "double", "long", "short", "unsigned",
    "signed", "const", "static", "extern", "volatile", "register",
    "struct", "union", "enum", "typedef", "sizeof",
    "return", "if", "else", "while", "for", "do", "switch", "case",
    "break", "continue", "default", "goto",
    "auto", "inline", "restrict",
    "NULL", "true", "false", "BOOL",
    "printf", "scanf", "malloc", "calloc", "realloc", "free",
    "strlen", "strcpy", "strncpy", "strcmp", "strncmp", "strcat",
    "memcpy", "memset", "memmove",
    "fopen", "fclose", "fread", "fwrite", "fprintf", "fscanf", "fgets", "fputs",
    "puts", "gets", "getchar", "putchar",
    "abs", "rand", "srand", "exit", "atol", "atof", "atoi",
    "qsort", "bsearch",
    "include", "define", "ifdef", "ifndef", "endif", "pragma",
    NULL
};

static const char *keywords_cpp[] = {
    "class", "public", "private", "protected", "virtual", "override",
    "new", "delete", "this", "namespace", "using", "template",
    "typename", "auto", "nullptr", "constexpr", "constexpr",
    "std", "string", "vector", "map", "set", "pair",
    "cout", "cin", "endl",
    NULL
};

static const char *keywords_py[] = {
    "def", "class", "return", "if", "elif", "else", "while", "for",
    "in", "import", "from", "as", "with", "try", "except", "finally",
    "raise", "pass", "break", "continue", "and", "or", "not", "is",
    "None", "True", "False", "print", "self", "lambda", "yield",
    "global", "nonlocal", "assert", "del", "async", "await",
    NULL
};

static int word_eq(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return 0;
        a++; b++;
    }
    return (*a == *b);
}

static int is_keyword(const char *word, const char **list) {
    for (int i = 0; list[i]; i++)
        if (word_eq(word, list[i])) return 1;
    return 0;
}

void highlight_init(void) {}

Color highlight_get_color(const char *word)
{
    if (is_keyword(word, keywords_c))
        return CLR_GREEN;

    if (is_keyword(word, keywords_cpp))
        return CLR_CYAN;

    if (is_keyword(word, keywords_py))
        return CLR_YELLOW;

    return CLR_DEFAULT;
}

Color highlight_get_char_color(char c) {
    if (c == '#') return CLR_GREEN;
    if (c == '"') return CLR_RED;
    if (c == '\'') return CLR_RED;
    return 0;
}
