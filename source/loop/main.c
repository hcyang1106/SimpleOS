#include "lib_syscall.h"
#include <stdio.h>
#include <string.h>
#include "main.h"
#include <stdlib.h>
#include <getopt.h>
#include <sys/file.h>
#include "fs/file.h"
#include "dev/tty.h"

int main(int argc, char **argv) {
    if (argc == 1) {
        char buf[128];
        fgets(buf, sizeof(buf), stdin); // null char auto added
        buf[strlen(buf)-1] = '\0';
        puts(buf);
        return 0;
    }

    int count = 1;
    char ch = '\0';
    while ((ch = getopt(argc, argv, "n:h")) != -1) { // mind the parenthesis
        switch(ch) {
            case 'n':
                count = atoi(optarg);
                break;
            case 'h':
                puts("echo -- echo message");
                optind = 1;
                return 0;
            default:
                // it seems that optarg here will always be a null pointer
                // if (optarg) {
                //     fprintf(stderr, ESC_COLOR_ERROR"Unknown option in echo: %s"ESC_COLOR_DEFAULT"\n", optarg);
                // }
                optind = 1;
                return -1;
        }
    }

    // after the code above, optidx should locate at the string ready to be echoed
    if (optind > argc - 1) {
        fprintf(stderr, "Emtpy string in echo""\n");
        optind = 1;
        return -1;
    }
    char *echo_string = argv[optind];
    for (int i = 0; i < count; i++) {
        puts(echo_string);
    }

    optind = 1;
    return 0;
}