#ifndef UTIL_H
#define UTIL_H

#define ASCII_RESET   "\033[0m"
#define ASCII_BOLD    "\033[1m"
#define ASCII_GREEN   "\033[32m"
#define ASCII_YELLOW  "\033[33m"
#define ASCII_RED     "\033[31m"
#define ASCII_DARKRED "\033[38;5;52m"
#define CURSOR_HIDE   "\033[?25l"
#define CURSOR_SHOW   "\033[?25h"

#define ASCII_INFO  "\033[1;35m"
#define ASCII_ERROR "\033[1;31m"

// like hello@1.0 or just hello
// name is for out
// version is for out
void parse_pkg_string(const char *input, char **name, char **version);

#endif // UTIL_H