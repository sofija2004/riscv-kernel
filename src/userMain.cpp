#include "../h/syscall_c.h"

// Minimal application entry point for the portfolio build.
// The original course test suite is intentionally not included.
void userMain() {
    const char* message = "RISC-V kernel: user application started.\n";
    while (*message) putc(*message++);
}
