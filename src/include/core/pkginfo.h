#ifndef PKG_INFO_H
#define PKG_INFO_H

#include <stdbool.h>
#include <stdint.h>

#define PKG_SUPPORTS_X64 (1 << 0)
#define PKG_SUPPORTS_X86 (1 << 1)
#define PKG_SUPPORTS_ARM (1 << 2)

typedef struct pkg_info_proc {
    char *proc_name;
    char *bash_code;
    struct pkg_info_proc *next;
} pkg_info_proc_t;

typedef struct pkg_info_var {
    char *name;
    char *value;
    struct pkg_info_var *next;
} pkg_info_var_t;

typedef struct pkg_info {
    char *name;
    char *version;
    char *description;
    char *maintainer;
    char *license;
    char *filepath;
    pkg_info_proc_t *procs;
    pkg_info_var_t *vars;

    bool is_source_pkg;

    uint64_t size;
} pkg_info_t;

pkg_info_t *parse_pkgbuild(const char *file_path);

#endif // PKG_INFO_H