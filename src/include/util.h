#ifndef UTIL_H
#define UTIL_H

// like hello@1.0 or just hello

// name is for out
// version is for out
void parse_pkg_string(const char *input, char **name, char **version);

#endif // UTIL_H