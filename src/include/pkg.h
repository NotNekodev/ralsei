#ifndef PKG_H
#define PKG_H

#include <stdint.h>
#include <time.h>

#define PKG_SUPPORTS_X64 (1 << 0)
#define PKG_SUPPORTS_X86 (1 << 1)
#define PKG_SUPPORTS_ARM (1 << 2)

typedef struct ral_pkg {
    char *name;
    char *version;
    char *description;

    uint8_t supported_arches_bitmask;

    char *author;
    char *license;

    char **dependencies;

    char *post_install_script_url;
    char *repo_url; // from which repo it has been installed

    time_t uploaded;
    time_t last_changed;
} ral_pkg_t;

// version can be null if the newest should be installed
ral_pkg_t *find_package(const char *name, const char *version);

int install_package(ral_pkg_t *pkg);
int install_local_tarball(const char *path, const char *root);

#endif // PKG_H