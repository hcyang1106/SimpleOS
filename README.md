# LaiohOS
LaiohOS is a simple, educational operating system designed to help users learn the core concepts of operating system development and operation.

## Operating System Loading Process

### Introduction
The loading of the Operating System (OS) into memory is an essential step in a computer's boot sequence. This section outlines the initial storage of the OS on the disk, its loading into memory, and the key stages in this process.

### Booting Sequence

#### BIOS and Boot Sector Validation
- **Storage**: Initially, the OS resides on the disk.
- **Power-On**: When the computer is turned on, the Basic Input/Output System (BIOS) checks the disk.
- **Validation Check**: BIOS reads the last two bytes of the first disk sector. If they match `0xAA` and `0x55`, it confirms the code is valid, a necessary condition to proceed with loading.

#### Memory Loading and Execution
- **Loading**: Post-validation, BIOS loads the first sector into memory at address `0x7c00`.
- **Execution**: BIOS then transfers control to address `0x7c00`, executing the code present there.

#### Boot Sector Limitations and Loader's Role
- **Size Constraint**: The boot sector holds only 512 bytes, limiting its operational scope.
- **Role of Loader**: The boot sector primarily loads a "loader," which then handles complex tasks such as:
  - Switching to protected mode.
  - Loading the kernel, and more.

### Frequently Asked Questions (FAQs)

#### What is Protected Mode?
- **Answer**: Protected mode is a CPU operational mode enabling the addressing of more than 1MB of memory and providing robust program isolation to prevent mutual interference.

#### Why do Intel CPUs start in Real Mode and then switch to Protected Mode?
- **Answer**: The primary reason is **Backward Compatibility**. CPUs featuring protected mode were introduced into an ecosystem abundant with real mode software. Starting in real mode ensures compatibility, allowing older systems and software to run on newer CPUs.

---

