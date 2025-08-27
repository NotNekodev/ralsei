#include "pkg.h"

#include "pkginfo.h"
#include "util.h"
#include <archive.h>
#include <archive_entry.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <threads.h>
#include <time.h>
#include <unistd.h>

static int64_t tmp_filelist_counter    = 0;
static int64_t tmp_ral_archive_counter = 0;

static int copy_data(struct archive *ar, struct archive *aw) {
    const void *buff;
    size_t size;
    int64_t offset;
    int r;

    for (;;) {
        r = archive_read_data_block(ar, &buff, &size, &offset);
        if (r == ARCHIVE_EOF)
            return ARCHIVE_OK;
        if (r != ARCHIVE_OK)
            return r;
        r = archive_write_data_block(aw, buff, size, offset);
        if (r != ARCHIVE_OK) {
            fprintf(stderr, "archive_write_data_block error: %s\n",
                    archive_error_string(aw));
            return r;
        }
    }
}

void *find_package(const char *name, const char *version) {
    (void)name;
    (void)version;
    return NULL;
}

int install_package(const char *pkg, const char *version) {
    return 0;
}

int install_dotral_pkg(const char *path, const char *root) {
    // make libarchive unarchive the .tar.zst into
    // /tmp/ralsei/{tmp_counter_thingy}

    if (root == NULL) {
        root = "/";
    }

    struct archive *a;
    struct archive *ext;
    struct archive_entry *entry;
    int r;

    char dest[512];
    snprintf(dest, sizeof(dest), "/tmp/ralsei/%ld", tmp_ral_archive_counter);

    mkdir(dest, 0755);

    a = archive_read_new();

    archive_read_support_format_tar(a);
    archive_read_support_filter_zstd(a);

    if ((r = archive_read_open_filename(a, path, 10240))) {
        fprintf(stderr, "Could not open %s: %s\n", path,
                archive_error_string(a));
        return 1;
    }

    ext = archive_write_disk_new();
    archive_write_disk_set_options(
        ext, ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL |
                 ARCHIVE_EXTRACT_FFLAGS);
    archive_write_disk_set_standard_lookup(ext);

    while (1) {
        r = archive_read_next_header(a, &entry);
        if (r == ARCHIVE_EOF)
            break;
        if (r < ARCHIVE_OK)
            fprintf(stderr, "%s\n", archive_error_string(a));
        if (r < ARCHIVE_WARN)
            return 1;

        const char *current_path = archive_entry_pathname(entry);
        char fullpath[4096];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dest, current_path);
        archive_entry_set_pathname(entry, fullpath);

        r = archive_write_header(ext, entry);
        if (r < ARCHIVE_OK)
            fprintf(stderr, "%s\n", archive_error_string(ext));
        else if (archive_entry_size(entry) > 0) {
            const void *buff;
            size_t size;
            la_int64_t offset;

            while (1) {
                r = archive_read_data_block(a, &buff, &size, &offset);
                if (r == ARCHIVE_EOF)
                    break;
                if (r < ARCHIVE_OK)
                    fprintf(stderr, "%s\n", archive_error_string(a));
                if (r < ARCHIVE_WARN)
                    return 1;
                r = archive_write_data_block(ext, buff, size, offset);
                if (r < ARCHIVE_OK)
                    fprintf(stderr, "%s\n", archive_error_string(ext));
                if (r < ARCHIVE_WARN)
                    return 1;
            }
        }
        r = archive_write_finish_entry(ext);
        if (r < ARCHIVE_OK)
            fprintf(stderr, "%s\n", archive_error_string(ext));
        if (r < ARCHIVE_WARN)
            return 1;
    }

    archive_read_close(a);
    archive_read_free(a);
    archive_write_close(ext);
    archive_write_free(ext);

    const char *filename = strrchr(path, '/');
    if (filename)
        filename++;
    else
        filename = path;

    char ral_file_name[256];
    strncpy(ral_file_name, filename, sizeof(ral_file_name) - 1);
    ral_file_name[sizeof(ral_file_name) - 1] = '\0';

    char *extfile = strrchr(ral_file_name, '.');
    if (extfile && strcmp(extfile, ".ral") == 0) {
        *extfile = '\0';
    }

    printf("pkg_name: %s\n", ral_file_name);

    char pkgbuild_path_buffer[512];
    snprintf(pkgbuild_path_buffer, sizeof(pkgbuild_path_buffer),
             "/tmp/ralsei/%ld/PKGBUILD", tmp_ral_archive_counter);

    pkg_info_t *pkg_info = parse_pkgbuild(pkgbuild_path_buffer);

    // loop over every proc and find "deps" and "post_install" proc is a linked
    // list
    pkg_info_proc_t *deps         = NULL;
    pkg_info_proc_t *post_install = NULL;
    pkg_info_proc_t *build        = NULL;

    pkg_info_proc_t *current = pkg_info->procs;
    while (current) {
        if (strcmp(current->proc_name, "deps") == 0) {
            deps = current;
        } else if (strcmp(current->proc_name, "post_install") == 0) {
            post_install = current;
        }

        if (pkg_info->is_source_pkg &&
            strcmp(current->proc_name, "build") == 0) {
            build = current;
        }
        current = current->next;
    }

    pid_t deps_shell_pid = fork();
    if (deps_shell_pid == 0) {
        char *deps_argv[] = {"/bin/bash", "-c", deps->bash_code, NULL};
        execv(deps_argv[0], deps_argv);
        perror("execv");
        exit(EXIT_FAILURE);
    } else if (deps_shell_pid < 0) {
        perror("fork");
        return 1;
    } else {
        waitpid(deps_shell_pid, NULL, 0);
    }

    if (pkg_info->is_source_pkg && build) {
        pid_t build_shell_pid = fork();
        if (build_shell_pid == 0) {
            char *build_argv[] = {"/bin/bash", "-c", build->bash_code, NULL};
            execv(build_argv[0], build_argv);
            perror("execv");
            exit(EXIT_FAILURE);
        } else if (build_shell_pid < 0) {
            perror("fork");
            return 1;
        } else {
            waitpid(build_shell_pid, NULL, 0);
        }
    } else {
        char binary_path_buffer[512];
        snprintf(binary_path_buffer, sizeof(binary_path_buffer),
                 "/tmp/ralsei/%ld/%s.tar.zst", tmp_ral_archive_counter,
                 ral_file_name);
        printf("test");
        install_local_tarball(binary_path_buffer, root);
    }

    pid_t post_install_shell_pid = fork();
    if (post_install_shell_pid == 0) {
        char *post_install_argv[] = {"/bin/bash", "-c", post_install->bash_code,
                                     NULL};
        execv(post_install_argv[0], post_install_argv);
        perror("execv");
        exit(EXIT_FAILURE);
    } else if (post_install_shell_pid < 0) {
        perror("fork");
        return 1;
    } else {
        waitpid(post_install_shell_pid, NULL, 0);
    }

    printf("Installed %s", ral_file_name);

    atomic_fetch_add(&tmp_ral_archive_counter, 1);
    return 0;
}

void install_local_tarball(const char *path, const char *root) {
    int tmp_file_fd = install_local_binary_tar_zst(path, root);

    struct stat st;
    if (fstat(tmp_file_fd, &st) == -1) {
        perror("fstat");
        close(tmp_file_fd);
        return;
    }

    lseek(tmp_file_fd, 0, SEEK_SET);

    char read_buffer[st.st_size + 1];
    read(tmp_file_fd, read_buffer, st.st_size);

    read_buffer[st.st_size] = '\0';

    close(tmp_file_fd);

    char *filename = strrchr(path, '/');
    if (filename)
        filename++;
    else
        filename = strdup(path);

    char tar_file_name[256];
    strcpy(tar_file_name, filename);

    char *ext = strstr(tar_file_name, ".tar.gz");
    if (ext)
        *ext = '\0';

    char buffer[256];
    struct stat st2 = {0};
    snprintf(buffer, sizeof(buffer), "/var/lib/ralsei/pkgs/%s", tar_file_name);
    if (stat(buffer, &st2) != -1) {
        fprintf(stdout,
                ASCII_ERROR ">>> " ASCII_RESET
                            "The package %s is already installed! Please "
                            "resolve this conflict!\n",
                tar_file_name);
        exit(EXIT_FAILURE);
    }

    mkdir(buffer, 0755);

    snprintf(buffer, sizeof(buffer), "/var/lib/ralsei/pkgs/%s/files.list",
             tar_file_name);

    int filelist = open(buffer, O_CREAT | O_RDWR, 0644);
    if (filelist == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    write(filelist, read_buffer, st.st_size);

    close(filelist);
}

int install_local_binary_tar_zst(const char *path, const char *root) {
    if (root == NULL) {
        root = "/";
    }

    char resolved_root[PATH_MAX];
    if (!realpath(root, resolved_root)) {
        perror("realpath");
        return -1;
    }
    root = resolved_root;

    struct archive *a;
    struct archive *ext;
    struct archive_entry *entry;
    int r;

    a = archive_read_new();
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);

    if ((r = archive_read_open_filename(a, path, 10240))) {
        fprintf(stderr, "archive_read_open_filename failed: %s\n",
                archive_error_string(a));
        return -1;
    }

    ext = archive_write_disk_new();
    archive_write_disk_set_options(
        ext, ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL |
                 ARCHIVE_EXTRACT_FFLAGS);
    archive_write_disk_set_standard_lookup(ext);

    char tmp_filelist_name_buffer[256];
    snprintf(tmp_filelist_name_buffer, sizeof(tmp_filelist_name_buffer),
             "/var/lib/ralsei/tmp/filelist-%ld",
             atomic_fetch_add(&tmp_filelist_counter, 1));

    int tmp_filelist_fd =
        open(tmp_filelist_name_buffer, O_CREAT | O_RDWR, 0644);
    if (tmp_filelist_fd < 0) {
        perror("open");
        return -1;
    }

    while ((r = archive_read_next_header(a, &entry)) == ARCHIVE_OK) {
        const char *current_file = archive_entry_pathname(entry);

        char fullpath[PATH_MAX];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", root, current_file);
        archive_entry_set_pathname(entry, fullpath);

        dprintf(tmp_filelist_fd, "%s\n", fullpath);

        r = archive_write_header(ext, entry);
        if (r != ARCHIVE_OK) {
            fprintf(stderr, "archive_write_header error: %s\n",
                    archive_error_string(ext));
        } else {
            copy_data(a, ext);
            r = archive_write_finish_entry(ext);
            if (r != ARCHIVE_OK) {
                fprintf(stderr, "archive_write_finish_entry error: %s\n",
                        archive_error_string(ext));
            }
        }
    }

    archive_read_close(a);
    archive_read_free(a);
    archive_write_close(ext);
    archive_write_free(ext);

    return tmp_filelist_fd;
}

int uninstall_package(const char *name) {
    char pkg_dir[1024];
    snprintf(pkg_dir, sizeof(pkg_dir), "/var/lib/ralsei/pkgs/%s/", name);
    struct stat st;
    if (stat(pkg_dir, &st) != 0) {
        perror("stat 1");
        return -1;
    }

    char filelist_path[1040];
    snprintf(filelist_path, sizeof(filelist_path), "%s/files.list", pkg_dir);
    struct stat stfl;
    if (stat(filelist_path, &stfl) != 0) {
        perror("stat 2");
        return -1;
    }

    char *buffer = malloc(stfl.st_size + 1);
    if (!buffer) {
        perror("malloc");
        return -1;
    }

    int fd = open(filelist_path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        free(buffer);
        return -1;
    }

    ssize_t n = read(fd, buffer, stfl.st_size);
    if (n < 0) {
        perror("read");
        close(fd);
        free(buffer);
        return -1;
    }

    buffer[n] = '\0';
    close(fd);
    size_t line_count = 0;
    for (ssize_t i = 0; i < n; i++) {
        if (buffer[i] == '\n')
            line_count++;
    }

    if (n > 0 && buffer[n - 1] != '\n') {
        line_count++;
    }

    char **lines = malloc((line_count + 1) * sizeof(char *));
    if (!lines) {
        perror("malloc");
        exit(1);
    }

    size_t idx  = 0;
    char *start = buffer;
    for (ssize_t i = 0; i < n; i++) {
        if (buffer[i] == '\n') {
            buffer[i]    = '\0';
            lines[idx++] = start;
            start        = &buffer[i + 1];
        }
    }

    if (*start != '\0') {
        lines[idx++] = start;
    }

    lines[idx] = NULL;
    struct stat stat_for_line_files_only_one_because_i_love_my_life;
    for (size_t i = 0; lines[i]; i++) {
        if (stat(lines[i],
                 &stat_for_line_files_only_one_because_i_love_my_life) == 0) {
            if (S_ISREG(stat_for_line_files_only_one_because_i_love_my_life
                            .st_mode)) {
                remove(lines[i]);
            } else if (S_ISLNK(
                           stat_for_line_files_only_one_because_i_love_my_life
                               .st_mode)) {
                unlink(lines[i]);
                remove(lines[i]);
            }
        }
    }

    free(lines);
    free(buffer);

    rmdir(pkg_dir);
    return 0;
}