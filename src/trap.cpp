#include "../h/Riscv.hpp"
#include "../h/console_lib.hpp"
#include "../lib/hw.h"

#include "../h/MemoryAllocator.hpp"
#include "../h/KThread.hpp"
#include "../h/kconsole.hpp"
#include "../h/KSemaphore.hpp"
#include "../h/SleepList.hpp"
#include "../h/KernelConsole.hpp"

static const int CONSOLE_IRQ_NO = 10;

// Indeksi u okviru: frame[0]=sepc, frame[i]=x_i, frame[32]=sstatus.
static constexpr int SEPC = 0, A0 = 10, A1 = 11, A2 = 12, A3 = 13, A4 = 14;

enum SysCode : uint64 {
    SYS_MEM_ALLOC    = 0x01,
    SYS_MEM_FREE     = 0x02,
    SYS_THR_CREATE   = 0x11,
    SYS_THR_EXIT     = 0x12,
    SYS_THR_DISPATCH = 0x13,
    SYS_SEM_OPEN     = 0x21,
    SYS_SEM_CLOSE    = 0x22,
    SYS_SEM_WAIT     = 0x23,
    SYS_SEM_SIGNAL   = 0x24,
    SYS_SEM_WAIT_N   = 0x25,
    SYS_SEM_SIGNAL_N = 0x26,
    SYS_TIME_SLEEP   = 0x31,
    SYS_GETC         = 0x41,
    SYS_PUTC         = 0x42,
};

static void panic(uint64 scause) {
    kconsole::puts("\nPANIC: neobradjen izuzetak, scause=");
    kconsole::putHex(scause);
    kconsole::puts(" sepc=");
    kconsole::putHex(riscv::r_sepc());
    kconsole::puts(" stval=");
    kconsole::putHex(riscv::r_stval());
    kconsole::puts("\n");
    for (;;) {}
}

extern "C" void handleSupervisorTrap(uint64* frame) {
    uint64 scause = riscv::r_scause();

    if (scause == riscv::CAUSE_EXTERNAL_S) {
        int irq = plic_claim();
        if (irq == CONSOLE_IRQ_NO) {
            KernelConsole::handleInterrupt();
        }
        if (irq > 0) plic_complete(irq);

        // Konzolni prekid ne samo da puni bafer vec predstavlja i
        // tacku asinhronog preotimanja, kako zahteva postavka.
        if (irq == CONSOLE_IRQ_NO && KThread::running)
            KThread::dispatch(true);
        return;
    }

    if (scause == riscv::CAUSE_SOFTWARE_S) {
        // Otkucaj tajmera: potvrdi prekid, pa prepusti niti/vremenu.
        riscv::mc_sip(riscv::SIP_SSIP);
        KThread::tick(); // moze SINHRONO izgledati, a ASINHRONO prekinuti
        return;
    }

    if (scause == riscv::CAUSE_ECALL_U || scause == riscv::CAUSE_ECALL_S) {
        // Povratak iza ecall-a; sepc zivi u okviru (po niti!), ne u CSR-u.
        frame[SEPC] += 4;

        switch (frame[A0]) {
            case SYS_MEM_ALLOC:
                frame[A0] = (uint64)MemoryAllocator::alloc(frame[A1]);
                break;
            case SYS_MEM_FREE:
                frame[A0] = (uint64)(long)MemoryAllocator::release((void*)frame[A1]);
                break;
            case SYS_THR_CREATE: {
                KThread** handle = (KThread**)frame[A1];
                KThread::Body body = (KThread::Body)frame[A2];
                void* stackTop = (void*)frame[A4];
                KThread* t = nullptr;
                if (handle && body && stackTop)
                    t = KThread::create(body, (void*)frame[A3], stackTop);
                if (handle) *handle = t;
                frame[A0] = t ? 0 : (uint64)-1;
                break;
            }
            case SYS_THR_EXIT:
                frame[A0] = 0;
                KThread::exitRunning(); // ne vraca se
                break;
            case SYS_THR_DISPATCH:
                frame[A0] = 0;
                KThread::dispatch(true);
                break;
            case SYS_SEM_OPEN: {
                KSemaphore** handle = (KSemaphore**)frame[A1];
                KSemaphore* sem = handle
                                ? KSemaphore::open((unsigned)frame[A2])
                                : nullptr;
                if (handle) *handle = sem;
                frame[A0] = sem ? 0 : (uint64)-1;
                break;
            }
            case SYS_SEM_CLOSE: {
                KSemaphore* sem = (KSemaphore*)frame[A1];
                frame[A0] = KSemaphore::valid(sem)
                          ? (uint64)(long)sem->close()
                          : (uint64)(long)KSemaphore::ERR_BAD_HANDLE;
                break;
            }
            case SYS_SEM_WAIT:
            case SYS_SEM_WAIT_N: {
                KSemaphore* sem = (KSemaphore*)frame[A1];
                unsigned n = (frame[A0] == SYS_SEM_WAIT) ? 1u
                                                         : (unsigned)frame[A2];
                frame[A0] = KSemaphore::valid(sem)
                          ? (uint64)(long)sem->wait(n)
                          : (uint64)(long)KSemaphore::ERR_BAD_HANDLE;
                break;
            }
            case SYS_SEM_SIGNAL:
            case SYS_SEM_SIGNAL_N: {
                KSemaphore* sem = (KSemaphore*)frame[A1];
                unsigned n = (frame[A0] == SYS_SEM_SIGNAL) ? 1u
                                                           : (unsigned)frame[A2];
                frame[A0] = KSemaphore::valid(sem)
                          ? (uint64)(long)sem->signal(n)
                          : (uint64)(long)KSemaphore::ERR_BAD_HANDLE;
                break;
            }
            case SYS_TIME_SLEEP: {
                unsigned long t = frame[A1];
                if (t == 0) { frame[A0] = 0; KThread::dispatch(true); break; }
                frame[A0] = (uint64)(long)KThread::sleepRunning(t);
                break;
            }
            case SYS_GETC:
                frame[A0] = (uint64)(long)KernelConsole::getc();
                break;
            case SYS_PUTC:
                KernelConsole::putc((char)frame[A1]);
                frame[A0] = 0;
                break;
            default:
                frame[A0] = (uint64)-1;
        }

        return;
    }

    if (scause == riscv::CAUSE_ILLEGAL) {
        if (!KThread::runningIsPrivileged()) {
            kconsole::puts("Nedozvoljena instrukcija u korisnickoj niti.\n");
            // Prekrsilacka nit ne sme da nastavi iza instrukcije.
            KThread::exitRunning();
            return;
        }
        panic(scause);
    }

    panic(scause);
}
