#include "pkginfo.h"

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char *trim(char *s) {
    while (isspace((unsigned char)*s))
        s++;
    if (*s == '\0')
        return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end))
        *end-- = '\0';
    return s;
}

static void add_var(pkg_info_t *pkg, const char *name, const char *value) {
    pkg_info_var_t *v = malloc(sizeof(*v));
    v->name           = strdup(name);
    v->value          = strdup(value);
    v->next           = NULL;

    if (!pkg->vars) {
        pkg->vars = v;
    } else {
        pkg_info_var_t *last = pkg->vars;
        while (last->next)
            last = last->next;
        last->next = v;
    }
}

static void add_proc(pkg_info_t *pkg, const char *name, const char *bash) {
    pkg_info_proc_t *p = malloc(sizeof(*p));
    p->proc_name       = strdup(name);
    p->bash_code       = strdup(bash);
    p->next            = NULL;

    if (!pkg->procs) {
        pkg->procs = p;
    } else {
        pkg_info_proc_t *last = pkg->procs;
        while (last->next)
            last = last->next;
        last->next = p;
    }
}

char *remove_comments(const char *in) {
    size_t len = strlen(in);
    char *out  = malloc(len + 1);
    if (!out)
        return NULL;

    char *curr      = out;
    const char *cin = in;

    while (*cin) {
        if (*cin == '#') {
            while (*cin && *cin != '\n')
                cin++;
        } else {
            *curr++ = *cin++;
        }
    }

    *curr = '\0';
    return out;
}

pkg_info_t *parse_pkgbuild(const char *file_path) {
    int fd = open(file_path, O_RDONLY);
    if (fd == -1)
        return NULL;

    struct stat st;
    if (fstat(fd, &st) == -1) {
        close(fd);
        return NULL;
    }

    pkg_info_t *pkg = calloc(1, sizeof(pkg_info_t));
    if (!pkg) {
        close(fd);
        return NULL;
    }
    pkg->size = st.st_size;

    char *buffer = malloc(st.st_size + 1);
    if (!buffer) {
        free(pkg);
        close(fd);
        return NULL;
    }

    ssize_t n = read(fd, buffer, st.st_size);
    close(fd);
    if (n < 0) {
        free(pkg);
        free(buffer);
        return NULL;
    }
    buffer[n] = '\0';

    char *clean = remove_comments(buffer);
    free(buffer);
    if (!clean) {
        free(pkg);
        return NULL;
    }

    char *saveptr;
    char *line = strtok_r(clean, "\n", &saveptr);
    while (line) {
        char *t = trim(line);

        if (*t == '\0') {
            line = strtok_r(NULL, "\n", &saveptr);
            continue;
        }

        if (strncmp(t, "proc ", 5) == 0) {
            char *name = t + 5;
            while (*name && !isspace((unsigned char)*name))
                name++;
            *name = '\0';
            name  = trim(t + 5);

            size_t cap = 256, len = 0;
            char *code = malloc(cap);
            code[0]    = '\0';

            while ((line = strtok_r(NULL, "\n", &saveptr))) {
                char *body = trim(line);
                if (strcmp(body, "}") == 0)
                    break;

                size_t blen = strlen(line);
                if (len + blen + 2 > cap) {
                    cap  *= 2;
                    code  = realloc(code, cap);
                }
                memcpy(code + len, line, blen);
                len         += blen;
                code[len++]  = '\n';
                code[len]    = '\0';
            }
            add_proc(pkg, name, code);
            free(code);
        } else {
            char *eq = strchr(t, '=');
            if (eq) {
                *eq         = '\0';
                char *name  = trim(t);
                char *value = trim(eq + 1);
                add_var(pkg, name, value);
            }
        }

        line = strtok_r(NULL, "\n", &saveptr);
    }

    pkg_info_var_t *var = pkg->vars;
    while (var) {
        printf("Setting env var: %s=%s\n", var->name, var->value);
        setenv(var->name, var->value, 1);
        var = var->next;
    }

    getenv("PKG_NAME") ? (pkg->name = strdup(getenv("PKG_NAME"))) : 0;
    getenv("PKG_VERSION") ? (pkg->version = strdup(getenv("PKG_VERSION"))) : 0;
    getenv("PKG_DESCRIPTION")
        ? (pkg->description = strdup(getenv("PKG_DESCRIPTION")))
        : 0;
    getenv("PKG_MAINTAINER")
        ? (pkg->maintainer = strdup(getenv("PKG_MAINTAINER")))
        : 0;
    getenv("PKG_LICENSE") ? (pkg->license = strdup(getenv("PKG_LICENSE"))) : 0;
    getenv("PKG_IS_SOURCE_PKG") ? (pkg->is_source_pkg = true)
                                : (pkg->is_source_pkg = false);

    pkg_info_proc_t *proc = pkg->procs;
    while (proc) {
        printf("PROC: %s\n", proc->proc_name);
        printf("%s\n", proc->bash_code);
        proc = proc->next;
    }

    free(clean);
    return pkg;
}

void pkg_info_destory(pkg_info_t *pkg) {
    if (!pkg)
        return;

    free(pkg->name);
    free(pkg->version);
    free(pkg->description);
    free(pkg->maintainer);
    free(pkg->license);
    free(pkg->filepath);

    pkg_info_var_t *var = pkg->vars;
    while (var) {
        unsetenv(var->name);
        pkg_info_var_t *next = var->next;
        free(var->name);
        free(var->value);
        free(var);
        var = next;
    }

    pkg_info_proc_t *proc = pkg->procs;
    while (proc) {
        pkg_info_proc_t *next = proc->next;
        free(proc->proc_name);
        free(proc->bash_code);
        free(proc);
        proc = next;
    }

    free(pkg);
}
