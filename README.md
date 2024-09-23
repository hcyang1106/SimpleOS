# SimpleOS
SimpleOS is an OS implementation that is used to enhance my understanding of OS concepts. The following explains how I implemented it.

## In Boot Sector
![Subdirectory Image](images/boot.jpeg)

The boot sector is the first sector on disk. Its job is to load the "Loader" to memory. After a computer is turned on, BIOS then loads the boot sector to memory address 0x7c00 and starts executing the code in it.

In the boot sector, we mainly do two things:
1. Loads the "Loader"  
2. Jumps to Loader address 0x8000

Q: Why do we put 0x55, 0xAA as the last two bytes in the boot sector?    

A: 0x55, 0xAA indicates that the boot sector is valid so that BIOS can proceed to load the code.

Q: How does boot load the Loader into memory?    

A: It uses BIOS interrupt to load the Loader from disk.

## In Loader
![Subdirectory Image](images/loader.png)

In loader, we do the following tasks:
1. Detect free memory spaces:  
    A BIOS interrupt is used to detect free memory spaces. The memory information is then passed to the Kernel for further usage.  

2. Enter the protected mode:  
    There are four steps to enter the protected mode.  
    a. Clear Interrupt  
    b. Open the A20 gate  
    c. Load GDT Table  
    d. Do a far jump (far jump is used to clear the pipelined instructions). 

3. Load the Kernel  
    We first load the elf file of Kernel to address 0x100000, and after that the code is extracted from the elf file and put at 0x10000.

Q: What is protected mode?    

A: Protected mode lets CPU access memory address higher than 1MB, and also provides protection, preventing programs from interfering with each other.

Q: Why do intel CPUs start from real mode first, and then enter protected mode later?    

A: Backward Compatibility. When CPUs with protected mode was released, there was already a significant base of software that ran in real mode. Starting in real mode ensures that older operating systems and software can still run on newer processors.

Q: Why don't we just load the kernel directly from disk?    

A: Sometimes there are spaces between sections (code, data, etc.). Therefore, the binary file could be large. If we load such a large file directly from disk, it may take a long time.

## Mutex
1. Initialization: The mutex is set to be unlocked with no owner, and a list is created to hold tasks that might have to wait for the lock.

2. Locking: When a task tries to lock the mutex, if no one owns it, the task becomes the owner. If someone else already owns the mutex, the task is put on a waiting list until the mutex becomes available.

3. Unlocking: When the owner releases the mutex, if other tasks are waiting, the first one in line is given ownership of the mutex and allowed to continue.

## Semaphore
1. Initialization: The semaphore is initialized with a specific count value, which represents the number of tasks that can proceed without waiting. A list is created to hold tasks that may need to wait.

2. Waiting (sem_wait): When a task wants to proceed, it checks if the semaphore count is greater than zero. If it is, the task decreases the count and continues. If the count is zero, the task is put on a waiting list, meaning it has to wait until it’s notified (when resources become available).

3. Notifying (sem_notify): When a resource is freed or made available, the first task in the waiting list is notified and allowed to proceed. If no tasks are waiting, the semaphore count is increased, allowing future tasks to continue without waiting.

## Task Structure
1. state: This enumerates the different states a task can be in, such as TASK_CREATED, TASK_RUNNING, TASK_SLEEP, etc. It tracks the current state of the task.

2. status: This holds a status value for the task, representing exit code.

3. pid: A unique identifier for the task, often called the process ID.

4. name[TASK_NAME_SIZE]: A character array that stores the task’s name, making it easier to identify or debug.

5. parent: A pointer to the parent task structure. This establishes a parent-child relationship between tasks.

6. tss: The Task State Segment (TSS) is a structure used by hardware for task switching. It holds various CPU register values for a task.

7. tss_sel: This is the TSS selector, used to switch between tasks relying on hardware-assisted task switching.

8. curr_tick: This is a countdown timer for the task. Once it reaches 0, it resets, triggering to reschedule.

9. sleep_tick: Used to track how long the task should remain in a sleep state. When the counter reaches zero, the task can wake up.

10. all_node: A list node that links this task to a global list of all tasks, making task management easier.

11. run_node: A list node used to manage the task in different lists (e.g., ready list, sleep list). This allows the task to move between states like "ready" or "sleeping."



---

