#include <stdio.h>
#include <strings.h>

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
    } else {

        if (strcasecmp(argv[1], "search") == 0) {
            printf("Searching for package: %s\n", argv[2]);
        } else if (strcasecmp(argv[1], "install") == 0) {
            printf("Installing package: %s\n", argv[2]);
        } else if (strcasecmp(argv[1], "remove") == 0) {
            printf("Removing package: %s\n", argv[2]);
        }
    }

    printf("probably weirdest edge case scenario lolol\n");
}