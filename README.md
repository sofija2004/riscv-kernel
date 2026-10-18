# Multithreaded RISC-V Kernel

An educational operating-system kernel for 64-bit RISC-V (RV64IMA), written in C++ and assembly. It implements memory allocation, threads, synchronization and console I/O on the QEMU `virt` machine.

## Features

- Custom block-based memory allocator.
- Thread creation, context switching, scheduling and timer-driven preemption.
- Semaphores, including wait/signal operations on multiple units.
- Sleeping and periodic threads.
- Interrupt-driven console input/output with kernel-managed buffers.
- 14 system calls exposed through a C-style API and C++ wrappers.
- Supervisor trap handling and a user application entry point.

## Build and run

Use the course Linux VM, or a Linux environment with GNU Make, a RISC-V GCC/binutils toolchain and `qemu-system-riscv64`. The build targets `-march=rv64ima_zicsr -mabi=lp64` (RV64IMA plus explicit CSR instructions for modern toolchains) and uses the supplied `lib/hw.lib` hardware support library.

```sh
make
make qemu
```

If automatic toolchain detection fails, select the prefix explicitly:

```sh
make TOOLPREFIX=riscv64-unknown-elf-
make qemu TOOLPREFIX=riscv64-unknown-elf-
```

`make qemu-gdb` starts QEMU for debugging; connect using `gdb-multiarch` and the port printed by Make. To exit QEMU manually, use Ctrl+A, then X.

## Structure

- `h/`: kernel interfaces and syscall APIs.
- `src/`: allocator, scheduler, threads, semaphores, console and trap handling.
- `src/userMain.cpp`: minimal user application that prints a startup message.
- `lib/`: course-provided hardware interfaces and libraries.
- `kernel.ld`: linker script.

## Portfolio packaging

The original course test directory is intentionally omitted. Since it also supplied `userMain`, a small application entry point is included so that removing the tests does not leave an unresolved symbol. The Makefile assembly rule uses `.S`, matching the source files on case-sensitive Linux filesystems, creates output directories and invokes the GCC driver for assembly. `RISCV_ARCH` defaults to `rv64ima_zicsr`; an older course toolchain may require `make RISCV_ARCH=rv64ima`. Kernel algorithms were not changed.

The original `LICENSE` is retained for the xv6-derived/course support code. The kernel uses that support library; this is not an entirely standalone hardware boot implementation.

## Verification

All C++ and assembly translation units compiled under the installed Ubuntu RISC-V toolchain. Linking failed inside BFD when linking the supplied `lib/hw.lib`, with an assertion in `elfnn-riscv.c`; both installed linkers (2.45.50 and 2.46) failed. No kernel image was produced and QEMU boot was **not verified**. This source package therefore needs a build/boot check in the original course VM/toolchain. Local include paths and the 14 syscall handlers were also inspected.
