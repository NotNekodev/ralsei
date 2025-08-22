#include "util.h"

#include <string.h>

void parse_pkg_string(const char *input, char **name, char **version) {
    char *at_sign = strchr(input, '@');
    if (at_sign) {
        *name    = strndup(input, at_sign - input);
        *version = strdup(at_sign + 1);
    } else {
        *name    = strdup(input);
        *version = NULL;
    }
}