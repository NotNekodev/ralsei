#include "pkg.h"

#include "util.h"
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

static int64_t tmp_filelist_counter = 0;

ral_pkg_t *find_package(const char *name, const char *version) {
    (void)name;
    (void)version;

    return NULL;
}

int install_local_dotral_pkg(const char *path) {
    return 0;
}

void install_local_tarball(const char *path, const char *root) {
    int tmp_file_fd = install_local_binary_tar_gz(path, root);
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

    // get only the tar files name so chop of the path infront of it and the
    // extension
    char *tar_file_name = strrchr(read_buffer, '/');
    if (tar_file_name) {
        tar_file_name++;
    } else {
        tar_file_name = read_buffer;
    }

    char *tar_file_ext = strrchr(tar_file_name, '.');
    if (tar_file_ext) {
        *tar_file_ext = '\0';
    }

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

    // we create <dir>/files.list and put the tar output there
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

int install_local_binary_tar_gz(const char *path, const char *root) {
    if (root == NULL) {
        root = "/";
    }

    char resolved_root[PATH_MAX];
    if (!realpath(root, resolved_root)) {
        perror("realpath");
        return -1;
    }
    root = resolved_root;

    int pipefd[2];
    pipe(pipefd);

    pid_t tar_pid = fork();
    if (tar_pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        execlp("tar", "tar", "-xvzf", path, "-C", root, "--keep-old-files",
               NULL);
        perror("Error executing tar subprocess");
        exit(EXIT_FAILURE);
    } else if (tar_pid < 0) {
        perror("Error creating tar subprocess");
        return -1;
    }
    close(pipefd[1]);

    int status;
    const char spinner[] = "|/-\\";
    int rotate_counter   = 0;
    int elapsed          = 0;

    printf(CURSOR_HIDE);
    fflush(stdout);

    while (1) {
        pid_t ret = waitpid(tar_pid, &status, WNOHANG);
        if (ret == -1) {
            perror("waitpid");
            break;
        } else if (ret == 0) {
            const char *color_spinner;
            const char *color_text = ASCII_RESET;
            if (elapsed < 5) {
                color_spinner = ASCII_BOLD ASCII_GREEN;
            } else if (elapsed < 10) {
                color_spinner = ASCII_BOLD ASCII_YELLOW;
            } else if (elapsed < 15) {
                color_spinner = ASCII_BOLD ASCII_RED;
            } else {
                color_spinner = ASCII_BOLD ASCII_DARKRED;
                color_text    = ASCII_BOLD ASCII_DARKRED;
            }
            printf("%s%c%s Installing \"%s\" -> \"%s\"%s\r", color_spinner,
                   spinner[rotate_counter % 4], color_text, path, root,
                   ASCII_RESET);
            fflush(stdout);
            rotate_counter++;
            sleep(1);
            elapsed++;
        } else {
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                printf("\r                                 \r" ASCII_INFO
                       ">>> " ASCII_RESET "Installed \"%s\" -> \"%s\"\n",
                       path, root);
            } else {
                printf("\r                                 \r" ASCII_ERROR
                       ">>> " ASCII_RESET "Error installing \"%s\"\n",
                       path);
            }
            break;
        }
    }

    size_t lines_size  = 16;
    size_t lines_count = 0;
    char **lines       = malloc(lines_size * sizeof(char *));
    if (!lines) {
        perror("malloc");
        exit(1);
    }

    size_t bufsize = 128;
    size_t buflen  = 0;
    char *buffer   = malloc(bufsize);
    if (!buffer) {
        perror("malloc");
        exit(1);
    }

    char c;
    ssize_t n;
    while ((n = read(pipefd[0], &c, 1)) > 0) {
        if (buflen + 1 >= bufsize) {
            bufsize *= 2;
            buffer   = realloc(buffer, bufsize);
            if (!buffer) {
                perror("realloc");
                exit(1);
            }
        }
        buffer[buflen++] = c;
        if (c == '\n') {
            buffer[buflen - 1] = '\0';
            if (lines_count >= lines_size) {
                lines_size *= 2;
                lines       = realloc(lines, lines_size * sizeof(char *));
                if (!lines) {
                    perror("realloc");
                    exit(1);
                }
            }
            lines[lines_count] = malloc(buflen);
            memcpy(lines[lines_count], buffer, buflen);
            lines_count++;
            buflen = 0;
        }
    }

    if (buflen > 0) {
        buffer[buflen] = '\0';
        if (lines_count >= lines_size) {
            lines_size *= 2;
            lines       = realloc(lines, lines_size * sizeof(char *));
            if (!lines) {
                perror("realloc");
                exit(1);
            }
        }
        lines[lines_count] = malloc(buflen + 1);
        memcpy(lines[lines_count], buffer, buflen + 1);
        lines_count++;
    }

    free(buffer);
    close(pipefd[0]);

    for (size_t i = 0; i < lines_count; i++) {
        const char *file_part = lines[i];
        if (strncmp(file_part, "./", 2) == 0) {
            file_part += 2;
        }
        char *real_path = malloc(strlen(root) + strlen(file_part) + 2);
        sprintf(real_path, "%s/%s", root, file_part);
        free(lines[i]);
        lines[i] = real_path;
        // printf("  - %s\n", lines[i]);
    }

    char tmp_filelist_name_buffer[256];
    snprintf(tmp_filelist_name_buffer, sizeof(tmp_filelist_name_buffer),
             "/var/lib/ralsei/tmp/filelist-%ld",
             atomic_fetch_add(&tmp_filelist_counter, 1));

    int tmp_filelist_fd =
        open(tmp_filelist_name_buffer, O_CREAT | O_RDWR, 0644);

    if (tmp_filelist_fd < 0) {
        perror("open");
        exit(1);
    }

    // write lines to the tmp_file
    for (size_t i = 0; i < lines_count; i++) {
        write(tmp_filelist_fd, lines[i], strlen(lines[i]));
        write(tmp_filelist_fd, "\n", 1);
    }

    for (size_t i = 0; i < lines_count; i++) {
        free(lines[i]);
    }
    free(lines);

    wait(NULL);

    printf(CURSOR_SHOW);
    fflush(stdout);

    return tmp_filelist_fd;
}