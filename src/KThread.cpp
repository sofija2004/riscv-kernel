#include "../h/KThread.hpp"
#include "../h/Scheduler.hpp"
#include "../h/MemoryAllocator.hpp"
#include "../h/Riscv.hpp"
#include "../h/SleepList.hpp"

// Placement new za konstrukciju u prostoru dobijenom od alokatora jezgra.
inline void* operator new(size_t, void* p) noexcept { return p; }

KThread* KThread::running = nullptr;
KThread* KThread::pendingReap = nullptr;
unsigned long KThread::sliceElapsed = 0;
unsigned KThread::liveUserThreads = 0;

// Korisnicki omotac tela niti (definisan u syscall_c.cpp, izvrsava se
// u U-rezimu): pozove body(arg), pa thread_exit().
extern "C" void userThreadWrapper(void (*body)(void*), void* arg);

static inline size_t toBlocks(size_t bytes) {
    return (bytes + MEM_BLOCK_SIZE - 1) / MEM_BLOCK_SIZE;
}

static inline uint64 lockInterrupts() {
    uint64 saved = riscv::r_sstatus();
    riscv::mc_sstatus(riscv::SSTATUS_SIE);
    return saved;
}

static inline void restoreInterrupts(uint64 saved) {
    if (saved & riscv::SSTATUS_SIE)
        riscv::ms_sstatus(riscv::SSTATUS_SIE);
}

KThread::KThread(Body body, void* arg, void* userStackTop, bool privileged)
    : body(body), arg(arg), userStackTop(userStackTop),
      stackBase(userStackTop ? (char*)userStackTop - DEFAULT_STACK_SIZE : nullptr),
      ctx(),
      state(State::READY), privileged(privileged), next(nullptr),
      need(0), blockStatus(0), sleepDelta(0) {}

KThread* KThread::create(Body body, void* arg, void* userStackTop,
                         bool privileged) {
    if (!body || !userStackTop) return nullptr;

    void* mem = MemoryAllocator::alloc(toBlocks(sizeof(KThread)));
    if (!mem) return nullptr;
    KThread* t = new (mem) KThread(body, arg, userStackTop, privileged);

    // Pocetni kontekst: prvi contextSwitch na ovu nit "vraca se" u
    // threadEntry sa vrha njenog (jedinog) steka.
    t->ctx.ra = (uint64)&threadEntry;
    t->ctx.sp = (uint64)userStackTop;

    Scheduler::put(t);
    if (!privileged) liveUserThreads++;
    return t;
}

KThread* KThread::createKernelThread(Body body, void* arg) {
    if (!body) return nullptr;

    void* stack = MemoryAllocator::alloc(toBlocks(DEFAULT_STACK_SIZE));
    if (!stack) return nullptr;

    KThread* thread = create(body, arg,
                             (char*)stack + DEFAULT_STACK_SIZE,
                             true);
    if (!thread) MemoryAllocator::release(stack);
    return thread;
}

KThread* KThread::wrapMain() {
    void* mem = MemoryAllocator::alloc(toBlocks(sizeof(KThread)));
    if (!mem) return nullptr;
    KThread* t = new (mem) KThread(nullptr, nullptr, nullptr, true);
    t->state = State::RUNNING;
    running = t;
    return t;
}

void KThread::dispatch(bool requeue) {
    // dispatch pozivaju i privilegovane interne niti. Rasporedjivac i
    // njegove intruzivne liste zato moraju ostati atomski prema tajmeru.
    uint64 savedStatus = lockInterrupts();
    sliceElapsed = 0; // nova odluka rasporedjivaca = novi odsecak
    KThread* old = running;
    if (!old) {
        restoreInterrupts(savedStatus);
        return;
    }
    if (requeue) {
        old->state = State::READY;
        Scheduler::put(old);
    }

    KThread* nxt = Scheduler::get();
    if (nxt == old) {
        old->state = State::RUNNING;
        restoreInterrupts(savedStatus);
        return;
    }
    if (!nxt) {
        // Tekuca je FINISHED ili BLOCKED, a spremnih nema: sistem je
        // u zastoju (deadlock korisnickog programa ili kraj sveta).
        for (;;) {}
    }

    running = nxt;
    nxt->state = State::RUNNING;
    if (old->state == State::FINISHED) pendingReap = old;

    contextSwitch(&old->ctx, &nxt->ctx);

    // Odavde nastavlja NIT KOJA SE VRATILA na procesor.
    reapPending();
    restoreInterrupts(savedStatus);
}

void KThread::exitRunning() {
    if (!running->privileged && liveUserThreads > 0) liveUserThreads--;
    running->state = State::FINISHED;
    dispatch(false);
}

int KThread::blockRunning() {
    running->state = State::BLOCKED;
    dispatch(false);
    // Ovde nastavljamo tek kada nas je neko vratio u red spremnih.
    return running->blockStatus;
}

int KThread::sleepRunning(unsigned long ticks) {
    running->state = State::SLEEPING;
    SleepList::put(running, ticks);
    dispatch(false);
    return running->blockStatus;
}

void KThread::tick() {
    SleepList::tick();
    if (++sliceElapsed >= DEFAULT_TIME_SLICE)
        dispatch(true); // asinhrono preuzimanje procesora
}

void KThread::threadEntry() {
    reapPending(); // promena konteksta koja nas je pokrenula mozda nosi dug

    KThread* t = running;
    if (t->privileged) {
        // Nova interna nit ne izlazi iz prekidne rutine preko sret, pa
        // prekide mora eksplicitno da dozvoli pre ulaska u svoje telo.
        riscv::ms_sstatus(riscv::SSTATUS_SIE);
        t->body(t->arg);
        riscv::mc_sstatus(riscv::SSTATUS_SIE);
        exitRunning();
    }

    // Ulazak u korisnicki rezim: povratna adresa sret-a je korisnicki
    // omotac tela, prethodni rezim U (SPP=0).
    riscv::w_sepc((uint64)&userThreadWrapper);
    uint64 ss = riscv::r_sstatus();
    ss &= ~riscv::SSTATUS_SPP;
    ss |= riscv::SSTATUS_SPIE;
    riscv::w_sstatus(ss);
    userEntry((uint64)t->body, (uint64)t->arg, (uint64)t->userStackTop);
}

void KThread::reapPending() {
    KThread* t = pendingReap;
    if (!t) return;
    pendingReap = nullptr;
    if (t->stackBase) MemoryAllocator::release(t->stackBase);
    MemoryAllocator::release(t);
}

bool KThread::hasLiveUserThreads() {
    return liveUserThreads != 0;
}

bool KThread::runningIsPrivileged() {
    return running && running->privileged;
}
