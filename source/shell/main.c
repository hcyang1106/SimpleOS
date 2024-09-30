#include "lib_syscall.h"
#include <stdio.h>
#include <string.h>
#include "main.h"
#include <stdlib.h>
#include <getopt.h>
#include <sys/file.h>
#include "fs/file.h"
#include "dev/tty.h"

static cli_t cli; // each process has a unique one, not shared (fork doesn't share global var)
static const char *prompt = "sh >> ";
// char cmd_buf[256];

static int do_help(int argc, char **argv) {
    const cli_cmd_t *p = cli.cmd_start;
    while (p < cli.cmd_end) {
        printf("%s %s\n", p->name, p->usage);
        p++;
    }
    return 0;
}

static int do_clear(int argc, char **argv) {
    printf("%s", ESC_CLEAR_SCREEN);
    printf("%s", ESC_MOVE_CURSOR(0, 0));
    return 0;
}

static int do_echo(int argc, char **argv) {
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
        fprintf(stderr, ESC_COLOR_ERROR"Emtpy string in echo"ESC_COLOR_DEFAULT"\n");
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

static int do_cp(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "invalid arguments, num of arg=%d\n", argc);
        goto cp_failed;
    }

    FILE *from = (FILE*)0, *to = (FILE*)0;
    from = fopen(argv[1], "rb"); // read mode, binary mode
    if (!from) {
        fprintf(stderr, "open src file failed\n");
        goto cp_failed;
    }

    to = fopen(argv[2], "wb");
    if (!to) {
        fprintf(stderr, "open dest file failed\n");
        goto cp_failed;
    }

    char *buf = malloc(sizeof(char)*255);
    int size;
    // these functions use buffer to store to 1024
    // after this they call system calls
    while ((size = fread(buf, sizeof(char), 255, from)) > 0) {
        fwrite(buf, sizeof(char), size, to);
    }

    free(buf);
    fclose(from);
    fclose(to);
    return 0;

cp_failed:
    if (from) {
        fclose(from);
    }
    if (to) {
        fclose(to);
    }
    return -1;
}

static int do_less(int argc, char **argv) {
    int line_mode = 0;

    char ch;
    while ((ch = getopt(argc, argv, "lh")) != -1) { // mind the parenthesis
        switch(ch) {
            case 'h':
                puts("less -- show file content");
                puts("Usage: less [-l] file");
                optind = 1;
                return 0;
            case 'l':
                line_mode = 1;
                break;
            default:
                // it seems that optarg here will always be a null pointer
                // if (optarg) {
                //     fprintf(stderr, ESC_COLOR_ERROR"Unknown option in echo: %s"ESC_COLOR_DEFAULT"\n", optarg);
                // }
                optind = 1;
                return -1;
        }
    }

    if (optind >= argc) {
        printf("less cmd without file name\n");
        return -1;
    }

    FILE *f = fopen(argv[optind], "r");
    if (!f) {
        printf("fopen failed. file=%s\n", argv[optind]);
    }

    char *buf = (char*)malloc(255);
    if (line_mode == 0) {
        while (fgets(buf, 255, f) != NULL) {
            fputs(buf, stdout);
        }
    } else {
        setvbuf(stdin, NULL, _IONBF, 0); 
        ioctl(0, TTY_CMD_ECHO, 0, 0);
        while (fgets(buf, 255, f) != NULL) {
            fputs(buf, stdout);
            char ch;
            while ((ch = fgetc(stdin))) {
                if (ch == 'n') {
                    break;
                } else if (ch == 'q') {
                    goto less_quit;
                }
            }
        }
    less_quit:
        line_mode = 0;
        ioctl(0, TTY_CMD_ECHO, 1, 0);
        setvbuf(stdin, NULL, _IOLBF, BUFSIZ); 
    }

    free(buf);
    fclose(f);
    optind = 1;
    return 0;
}

static int do_exit(int argc, char **argv) {
    exit(0);
    return 0;
}

static int do_ls(int argc, char **argv) {
    DIR *p_dir = opendir("temp");
    if (!p_dir) {
        printf("open dir failed");
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(p_dir)) != NULL) {
        strlwr(entry->name);
        printf("%c %s %d\n",
                entry->type == FILE_DIR ? 'd':'f',
                entry->name,
                entry->size
        );
    }

    closedir(p_dir);
    return 0;
}

static int do_rm(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "invalid arguments, num of arg=%d\n", argc);
        return -1;
    }

    int ret = unlink(argv[1]);
    if (ret < 0) {
        fprintf(stderr, "remove file failed\n");
        return ret;
    }

    return 0;
}

static const cli_cmd_t cmd_list[] = {
    {
        .name = "help",
        .usage = "help -- list supported commands",
        .do_func = do_help,
    },
    {
        .name = "clear",
        .usage = "clear -- clear screen",
        .do_func = do_clear,
    },
    {
        .name = "echo",
        .usage = "echo -- echo message",
        .do_func = do_echo,
    },
    {
        .name = "ls",
        .usage = "ls -- list directory",
        .do_func = do_ls,
    },
    {
        .name = "less",
        .usage = "less [-l] file -- show some file contents",
        .do_func = do_less,
    },
    {
        .name = "cp",
        .usage = "cp src dest -- copy file",
        .do_func = do_cp,
    },
    {
        .name = "rm",
        .usage = "rm file - remove file",
        .do_func = do_rm,
    },
    {
        .name = "quit",
        .usage = "quit -- quit from shell",
        .do_func = do_exit,
    },
};

static void cli_init(void) {
    cli.prompt = prompt;
    memset(cli.curr_input, 0, CLI_INPUT_SIZE);
    cli.cmd_start = cmd_list;
    cli.cmd_end = cmd_list + sizeof(cmd_list) / sizeof(cmd_list[0]);
}

static void show_prompt(void) {
    printf("%s", cli.prompt);
    fflush(stdout); // new lib has buffer
}

static const cli_cmd_t *find_builtin_cmd(char *name) {
    if (!cli.cmd_start || !cli.cmd_end) {
        return (cli_cmd_t*)0;
    }

    const cli_cmd_t *p;
    for (p = cli.cmd_start; p < cli.cmd_end; p++) {
        if (!strncmp(p->name, name, sizeof(name))) { // returns zero if same
            return p;
        }
    }
    return (cli_cmd_t*)0;
}

static int run_builtin_func(const cli_cmd_t *cmd, int argc, char**argv) {
    return cmd->do_func(argc, argv);
}

static int check_file_exist(const char *name) {
    char path[255];
    int fd = open(name, 0);
    if (fd < 0) {
        sprintf(path, "%s.elf", name);
        fd = open(path, 0);
        if (fd < 0) {
            return -1;
        }
    }
    close(fd);
    return 0;
}

static void run_exec_file(const char *path, int argc, char **argv) {
    int ret = fork();
    if (ret < 0) {
        fprintf(stderr, "fork failed %s", path);
    } else if (ret == 0) { // child
        // fprintf(stdout, "child process\n");
        // for (int i = 0; i < 10; i++) {
        //     fprintf(stdout, "child process\n");
        // }
        int err = execve(path, argv, (char *const *)0);
        if (err < 0) {
            fprintf(stderr, "exec failed...\n");
        }
        exit(-1);
    } else {
        int status;
        int pid = wait(&status);
        fprintf(stdout, "command %s result: status=%d, pid=%d\n", path, status, pid);
    }
}

int main(int argc, char **argv) {
#if 0
    // char* ret = sbrk(0);
    // ret = sbrk(100);
    // ret = sbrk(4096);
    // ret = sbrk(4096*2 + 200);
    // ret = sbrk(4096*5 + 1234);


    // write(1, "Hello SimpleOS write", 20);
    printf("Hello SimpleOS printf\n");
    printf("Hello_SimpleOS printf...\n");

    printf("abef\b\b\b\bcd\n");
    printf("abed\x7f;fg\n");

    printf("\0337Hello, word!\0338123\n");  // ESC 7,8 => 123lo, word!
    printf("\033[31;42mHello, word!\033[39;49m123\n");  // Hello, world! (red text, green background)
    printf("123\033[2DHello, word!\n");  // cursor move left 2, 1Hello, word!
    printf("123\033[2CHello, word!\n");  // cursor move right 2, 123  Hello, word!

    printf("\033[31m");  // ESC [pn m, Hello, world red, the rest is green 
    printf("\033[10;10H test!\n");  // position 10, 10, test!
    printf("\033[20;20H test!\n");
    printf("\033[32;25;39m123\n");  // ESC [pn m, Hello, world red, the rest is green

    printf("\033[2J");   // clear screen
    fflush(0); // used to 
#endif

    // for (int i = 0; i < argc; i++) {
    //     printf("arg: %s\n", (int)argv[i]);
    // }

    // fork();

    // yield();


    // the standard input or ouput of a process can
    // only be connected to one device!
    // that means if we only run one shell program,
    // we can't do input/output to other ttys
    open(argv[0], O_RDWR);
    dup(0);
    dup(0);

    cli_init();
    
    int pid = getpid();
    for (;;) {
        // gets reads until a newline (not included)
        // puts writes the string and adds a newline
        // gets(cmd_buf);
        // puts(cmd_buf);

        // printf("shell task id = %d\n", pid);
        // msleep(1000);
        show_prompt();

        // input string processing
        // gets(cli.curr_input); using gets is dangerous
        // fgets is possible to store newline (if length is long enough to put '\0' at the end)
        fgets(cli.curr_input, CLI_INPUT_SIZE, stdin);
        char *ch = strchr(cli.curr_input, '\r');
        if (ch) {
            *ch = '\0';
        }
        ch = strchr(cli.curr_input, '\n');
        if (ch) {
            *ch = '\0';
        }

        int argc = 0;
        char *argv[CLI_MAX_ARG_NUM];
        memset(argv, CLI_MAX_ARG_NUM, sizeof(char*));

        // strtok sets the delim to '\0' and return the position of token
        const char *delim = " ";
        char *token = strtok(cli.curr_input, delim);
        while (token) {
            argv[argc++] = token;
            token = strtok(NULL, delim);
        }

        if (argc == 0) {
            continue;
        }

        const cli_cmd_t *cmd = find_builtin_cmd(argv[0]);
        if (cmd) {
            run_builtin_func(cmd, argc, argv);
            continue;
        }

        int ret = check_file_exist(argv[0]);
        if (ret >= 0) {
            run_exec_file(argv[0], argc, argv);
            continue;
        }

        fprintf(stderr, ESC_COLOR_ERROR"Not a builtin command: %s"ESC_COLOR_DEFAULT"\n", argv[0]);
    }

}


// About Buffer when doing print

// In C, the \n newline character does not directly clear the buffer, but it can cause the buffer to be flushed (sent out) when using line buffering.

// Types of Buffers
// There are three types of buffers in the standard I/O library:

// Line Buffering:

// Commonly used for output to the terminal.
// The buffer is flushed when a newline character \n is encountered.
// Full Buffering:

// Commonly used for output to files.
// The buffer is flushed when it is full or when the fflush function is called.
// No Buffering:

// Commonly used for standard error output (stderr).
// The output is sent immediately without buffering.