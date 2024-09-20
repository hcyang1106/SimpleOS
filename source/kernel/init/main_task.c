// here we're trying to separate the main task code from kernel code
#include "lib_syscall.h"
#include "dev/tty.h"

int main_task(void) {
#if 0
    // it is important that after we link libapp with kernel,
    // the system calls would become kernel code, which is below 0x80000000
    // and we can't execute it under level 3 (e.g. getpid())
    int test = getpid();
    
    // for (;;) {
    //     print_msg("Task pid: %d", pid);
    //     // log_printf("main task"); // can't access kernel code now
    //     // sys_sleep(1000);
    //     msleep(1000);
    // }

    int ret = fork();
    if (ret < 0) {
        print_msg("create child proc failed.", 0);
    } else if (ret == 0) {
        print_msg("child!", 0);
        char *argv[] = {"arg0", "arg1", "arg2", "arg3"};
        execve("/shell.elf", argv, (char**)0);
    } else {
        print_msg("parent!", 0);
    }
#endif
    // for (int i = 0; i < TTY_NUM; i++) {
    for (int i = 0; i < 2; i++) {
        int ret = fork();
        if (ret < 0) {
            print_msg("fork failed in main task", 0);
            break; 
        } else if (ret == 0) {
            char tty_num[] = "/dev/tty?";
            tty_num[sizeof(tty_num) - 2] = i + '0';
            char *argv[] = {tty_num, (char*)0};
            execve("shell.elf", argv, (char**)0);
            while(1) {
                msleep(1000);
            }
        }
        
    }

    int pid = getpid();
    for (;;) {
        // msleep(1000);
        int status;
        wait(&status);
        print_msg("child process ends", 0);
    }

    return 0;
}
