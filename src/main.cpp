#include "../h/Riscv.hpp"
#include "../h/kconsole.hpp"
#include "../h/KThread.hpp"
#include "../h/MemoryAllocator.hpp"
#include "../h/syscall_c.h"
#include "../h/console_lib.hpp"
#include "../h/KernelConsole.hpp"
#include "../h/Scheduler.hpp"

void userMain();

static void userMainRunner(void*) {
    userMain();
}

static void idleBody(void*) {
    for (;;) {
        if (Scheduler::hasReady()) {
            KThread::dispatch(true);
        } else {
            // wfi se koristi samo kada zaista nema korisnog rada.
            asm volatile("wfi");
        }
    }
}

// Kraj rada QEMU virt masine. SiFive test uredjaj na 0x100000: upis
// 0x5555 gasi emulator (uredan izlaz iz -nographic sesije).
static void qemuShutdown() {
    *(volatile unsigned int*)0x100000 = 0x5555;
    for (;;) {}
}

int main() {
    riscv::w_stvec((uint64)&supervisorTrap);

    kconsole::puts("Kernel ziv! (main u S-rezimu)\n");

    // running mora postojati pre nego sto ijedan prekid moze pozvati
    // KThread::tick ili promenu konteksta.
    if (!KThread::wrapMain()) qemuShutdown();

    // Inicijalizuj hardver, ali prekide jos ne ukljucuj.
    consoleinit();
    plicinit();
    plicinithart();

    if (!KernelConsole::initialize()) qemuShutdown();
    if (!KThread::createKernelThread(&idleBody)) qemuShutdown();

    // userMain je prva korisnicka nit i ulazi u isti brojac kao svaka
    // nit koju aplikacija kasnije napravi pozivom thread_create.
    size_t stackBlocks = (DEFAULT_STACK_SIZE + MEM_BLOCK_SIZE - 1) /
                         MEM_BLOCK_SIZE;
    void* ustack = MemoryAllocator::alloc(stackBlocks);
    if (!ustack) qemuShutdown();
    if (!KThread::create(&userMainRunner, nullptr,
                         (char*)ustack + DEFAULT_STACK_SIZE)) {
        MemoryAllocator::release(ustack);
        qemuShutdown();
    }

    // Sve strukture i uvek spremna idle nit sada postoje; prekidi su
    // od ovog trenutka bezbedni.
    riscv::ms_sie((1UL << 9)); // SEIE: konzola
    riscv::ms_sie(riscv::SIE_SSIE); // SSIE: tajmer
    riscv::ms_sstatus(riscv::SSTATUS_SIE);

    while (KThread::hasLiveUserThreads()) thread_dispatch();

    // Poslednji korisnicki putc moze jos biti u izlaznom baferu.
    while (!KernelConsole::outputDrained()) thread_dispatch();

    kconsole::puts("Sve korisnicke niti su zavrsene, jezgro staje.\n");
    qemuShutdown();
    return 0;
}
