#include "../h/KSemaphore.hpp"
#include "../h/Scheduler.hpp"
#include "../h/MemoryAllocator.hpp"
#include "../h/Riscv.hpp"

inline void* operator new(size_t, void* p) noexcept { return p; }

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

KSemaphore::KSemaphore(unsigned init)
    : tag(TAG_LIVE), value(init), qHead(nullptr), qTail(nullptr) {}

KSemaphore* KSemaphore::open(unsigned init) {
    void* mem = MemoryAllocator::alloc(toBlocks(sizeof(KSemaphore)));
    if (!mem) return nullptr;
    return new (mem) KSemaphore(init);
}

bool KSemaphore::valid(KSemaphore* s) {
    return s != nullptr && s->tag == TAG_LIVE;
}

void KSemaphore::enqueue(KThread* t) {
    t->next = nullptr;
    if (qTail) qTail->next = t;
    else qHead = t;
    qTail = t;
}

KThread* KSemaphore::dequeue() {
    KThread* t = qHead;
    if (!t) return nullptr;
    qHead = t->next;
    if (!qHead) qTail = nullptr;
    t->next = nullptr;
    return t;
}

void KSemaphore::wakeHead(int status) {
    KThread* t = dequeue();
    t->blockStatus = status;
    t->state = KThread::State::READY;
    Scheduler::put(t);
}

int KSemaphore::wait(unsigned n) {
    uint64 savedStatus = lockInterrupts();
    if (value >= n) {
        value -= n;
        restoreInterrupts(savedStatus);
        return 0;
    }
    KThread* cur = KThread::running;
    cur->need = n;
    enqueue(cur);
    int status = KThread::blockRunning(); // status upisuje budilac
    restoreInterrupts(savedStatus);
    return status;
}

int KSemaphore::signal(unsigned n) {
    uint64 savedStatus = lockInterrupts();
    value += n;
    // Strogi FIFO: propustaj s pocetka reda dok god glava moze da prodje.
    while (qHead && qHead->need <= value) {
        value -= qHead->need;
        wakeHead(0);
    }
    restoreInterrupts(savedStatus);
    return 0;
}

int KSemaphore::close() {
    uint64 savedStatus = lockInterrupts();
    tag = 0; // od ovog trenutka rucka vise nije validna
    while (qHead) wakeHead(ERR_CLOSED);
    MemoryAllocator::release(this);
    restoreInterrupts(savedStatus);
    return 0;
}
