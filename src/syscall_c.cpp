#include "../h/syscall_c.h"

// Kodovi sistemskih poziva (prva kolona tabele iz speca 2026).
enum SysCode : unsigned long {
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

// Zajednicka operacija svih sistemskih poziva (spec: "funkcija opsteg
// oblika koja prima potpuno opste argumente i izvrsava ecall").
// Argumenti se vezuju direktno za registre a0..a4; povratna vrednost
// jezgra stize nazad kroz a0.
static unsigned long sysCall(unsigned long code,
                             unsigned long p1 = 0,
                             unsigned long p2 = 0,
                             unsigned long p3 = 0,
                             unsigned long p4 = 0) {
    register unsigned long a0 asm("a0") = code;
    register unsigned long a1 asm("a1") = p1;
    register unsigned long a2 asm("a2") = p2;
    register unsigned long a3 asm("a3") = p3;
    register unsigned long a4 asm("a4") = p4;
    asm volatile("ecall"
                 : "+r"(a0)
                 : "r"(a1), "r"(a2), "r"(a3), "r"(a4)
                 : "memory");
    return a0;
}

// --- Z1 ---

void* mem_alloc(size_t size) {
    if (size == 0) return nullptr;
    // C API dobija bajtove, ABI ocekuje blokove: zaokruzi navise.
    size_t blocks = (size + MEM_BLOCK_SIZE - 1) / MEM_BLOCK_SIZE;
    return (void*)sysCall(SYS_MEM_ALLOC, blocks);
}

int mem_free(void* ptr) {
    return (int)sysCall(SYS_MEM_FREE, (unsigned long)ptr);
}

// --- Z2 ---

int thread_create(thread_t* handle, void (*start_routine)(void*), void* arg) {
    // ABI thread_create ima 4. parametar: unapred alociran stek.
    void* stack = mem_alloc(DEFAULT_STACK_SIZE);
    if (!stack) return -1;
    // ABI ocekuje "poslednju lokaciju" steka - vrh, ne dno.
    unsigned long stackTop = (unsigned long)stack + DEFAULT_STACK_SIZE;
    int status = (int)sysCall(SYS_THR_CREATE, (unsigned long)handle,
                              (unsigned long)start_routine,
                              (unsigned long)arg, stackTop);

    // Vlasnistvo steka prelazi na jezgro samo posle uspesnog kreiranja.
    if (status < 0) mem_free(stack);
    return status;
}

int thread_exit()      { return (int)sysCall(SYS_THR_EXIT); }
void thread_dispatch() { sysCall(SYS_THR_DISPATCH); }

// --- Z3 ---

int sem_open(sem_t* handle, unsigned init) {
    return (int)sysCall(SYS_SEM_OPEN, (unsigned long)handle, init);
}
int sem_close(sem_t id)  { return (int)sysCall(SYS_SEM_CLOSE, (unsigned long)id); }
int sem_wait(sem_t id)   { return (int)sysCall(SYS_SEM_WAIT, (unsigned long)id); }
int sem_signal(sem_t id) { return (int)sysCall(SYS_SEM_SIGNAL, (unsigned long)id); }
int sem_wait_n(sem_t id, unsigned n) {
    return (int)sysCall(SYS_SEM_WAIT_N, (unsigned long)id, n);
}
int sem_signal_n(sem_t id, unsigned n) {
    return (int)sysCall(SYS_SEM_SIGNAL_N, (unsigned long)id, n);
}

// --- Z4 ---

int time_sleep(time_t t) { return (int)sysCall(SYS_TIME_SLEEP, t); }
char getc()              { return (char)sysCall(SYS_GETC); }
void putc(char c)        { sysCall(SYS_PUTC, (unsigned long)c); }

// Omotac tela korisnicke niti: izvrsava se u korisnickom rezimu kao
// prva "funkcija" niti (jezgro sret-uje na nju), pa garantuje da se
// nit uredno zavrsi sistemskim pozivom i kada telo samo istekne.
extern "C" void userThreadWrapper(void (*body)(void*), void* arg) {
    body(arg);
    thread_exit();
}
