#define _GNU_SOURCE
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "include/pkg.h"
#include "include/util.h"

#include <getopt.h>
#include <unistd.h>

#include <sys/stat.h>

static int tarball;
static char *root = NULL;

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("###############################################################"
               "##########\n");
        printf("#* I'll do anything for you...                                 "
               "         #\n");
        printf("#* Well if that anything is installing, removing               "
               "         #\n");
        printf("#* Or searching packages !                                     "
               "         #\n");
        printf("###############################################################"
               "##########\n");
        printf(""
               "======================= Ralsei's Manual (Abridged) "
               "======================\n"
               "Searching for packages: ral search <query>\n"
               "Installing packages: ral install <package name>\n"
               "Removing packages: ral remove <package name>\n"
               "\n");

        printf("Hope you have a nice day managing packages!\n"
               "In case you need more information you can call me with --help\n"
               "for the full manual.\n"
               "It's me, Ralsei version 0.0.1: \"Fluffy beginnings\"!\n");
        exit(EXIT_SUCCESS);
    }

    if (geteuid() != 0) {
        fprintf(stderr,
                ASCII_ERROR ">>> " ASCII_RESET "Please run ralsei as root!\n");
        exit(EXIT_FAILURE);
    }

    struct stat st = {0};
    if (stat("/var/lib/ralsei", &st) == -1) {
        fprintf(stdout, ASCII_WARN ">>> " ASCII_RESET
                                   "Ralsei has been run the first time on this "
                                   "system. Creating /var/lib/ralsei\n");
        mkdir("/var/lib/ralsei", 0755);
        mkdir("/var/lib/ralsei/pkgs", 0755);
        mkdir("/var/lib/ralsei/tmp", 0755);
    }

    int opt;
    int opt_idx = 0;

    static struct option long_options[] = {
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'v'},
        {"tarball", no_argument, NULL, 't'},
        {"root", required_argument, NULL, 'r'},
        {0, 0, 0, 0}};

    while ((opt = getopt_long(argc, argv, "hvtr:", long_options, &opt_idx)) !=
           -1) {
        switch (opt) {
        case 'h':
            printf("Usage: %s [-h|--help] action [-t|--tarball] package [if "
                   "tarball: -r|--root <path>]\n",
                   argv[0]);
            break;
        case 'v':
            printf("Version: 0.0.1\n");
            break;
        case 't':
            tarball = 1;
            break;
        case 'r':
            root = optarg;
            break;
        default:
            fprintf(stderr,
                    ASCII_ERROR ">>> " ASCII_RESET
                                "Ralsei didn't understand the argument: '%s'! "
                                "Run him with --help for more information.\n",
                    argv[optind]);
            exit(EXIT_FAILURE);
        }
    }

    for (int i = optind; i < argc; i++) {
        if (strcasecmp(argv[i], "install") == 0) {
            if (tarball) {
                if (argv[i + 1] == NULL) {
                    fprintf(stderr,
                            "Error: No path specified for the tarball.\n");
                    exit(EXIT_FAILURE);
                }
                install_local_tarball(argv[i + 1], root);
            } else {
                printf("Installing package %s\n", argv[i + 1]);
            }
            i++;
        } else {
            fprintf(stderr,
                    ASCII_ERROR
                    ">>> " ASCII_RESET
                    "Ralsei did not understand the command: '%s'!\n",
                    argv[i]);
            exit(EXIT_FAILURE);
        }
    }
}