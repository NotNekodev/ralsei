#include "pkg.h"

#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include <time.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>

ral_pkg_t *find_package(const char *name, const char *version) {
    // TODO

    return NULL;
}

int install_local_tarball(const char *path, const char *root) {
    if (root == NULL) {
        root = "/";
    }

    pid_t tar_pid = fork();
    if (tar_pid == 0) {
        execlp("tar", "tar", "-xzf", path, "-C", root, "--keep-old-files",
               NULL);
        perror("Error executing tar subprocess");
        exit(EXIT_FAILURE);
    } else if (tar_pid < 0) {
        perror("Error creating tar subprocess");
        return -1;
    }

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
                       ">>> " ASCII_RESET "Error installing "
                       "\"%s\"\n",
                       path);
            }
            break;
        }
    }

    printf(CURSOR_SHOW);
    fflush(stdout);

    return 0;
}