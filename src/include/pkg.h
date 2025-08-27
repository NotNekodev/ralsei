#ifndef PKG_H
#define PKG_H

#include <stdbool.h>
#include <stdint.h>

// version can be null if the newest should be installed
void *find_package(const char *name, const char *version);

int install_package(const char *pkg, const char *version);
int install_dotral_pkg(const char *path, const char *root);
void install_local_tarball(const char *path, const char *root);
int install_local_binary_tar_zst(const char *path, const char *root);

int uninstall_package(const char *name);

#endif // PKG_H