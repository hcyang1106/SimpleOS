#ifndef OS_CFG_H
#define OS_CFG_H

#define GDT_TABLE_SIZE 256
#define IDT_TABLE_SIZE 128

#define KERNEL_SELECTOR_CS (1 * 8)
#define KERNEL_SELECTOR_DS (2 * 8)
#define SELECTOR_SYSCALL (3 * 8)
#define KERNEL_STACK_SIZE (8 * 1024)

#define OS_TICK_MS 10 // clock times per ms

#define OS_VERSION "1.0.0"

#define TASK_NUM 128

#define SECTOR_SIZE 512

#define ROOT_DEV DEV_DISK, 0xb1

#endif
