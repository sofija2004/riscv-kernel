#include "../h/syscall_cpp.hpp"

// Globalni new/delete korisnickog sloja preko sistemskih poziva.
void* operator new(size_t n)            { return mem_alloc(n); }
void operator delete(void* p) noexcept  { mem_free(p); }
void* operator new[](size_t n)          { return mem_alloc(n); }
void operator delete[](void* p) noexcept { mem_free(p); }
// sized varijante (C++14) - prosledjuju na obicne
void operator delete(void* p, size_t) noexcept   { mem_free(p); }
void operator delete[](void* p, size_t) noexcept { mem_free(p); }

// --- Thread: tanak objektni omotac oko C API-ja ---

Thread::Thread(void (*body)(void*), void* arg)
    : myHandle(nullptr), body(body), arg(arg) {}

Thread::Thread() : myHandle(nullptr), body(nullptr), arg(nullptr) {}

Thread::~Thread() {}

void Thread::runHelper(void* self) {
    ((Thread*)self)->run();
}

int Thread::start() {
    if (body) return thread_create(&myHandle, body, arg);
    return thread_create(&myHandle, &runHelper, this);
}

void Thread::dispatch() { thread_dispatch(); }
int Thread::sleep(time_t t) { return time_sleep(t); }

// --- Semaphore (jezgro implementira u Z3; omotac je vec kompletan) ---

Semaphore::Semaphore(unsigned init) : myHandle(nullptr) {
    sem_open(&myHandle, init);
}
Semaphore::~Semaphore() { sem_close(myHandle); }
int Semaphore::wait()   { return sem_wait(myHandle); }
int Semaphore::signal() { return sem_signal(myHandle); }

// --- PeriodicThread (puna semantika stize sa Z4/time_sleep) ---

PeriodicThread::PeriodicThread(time_t period)
    : Thread(), period(period) {}

void PeriodicThread::run() {
    while (period) {
        periodicActivation();
        sleep(period);
    }
}
void PeriodicThread::terminate() { period = 0; }

// --- Console ---

char Console::getc()       { return ::getc(); }
void Console::putc(char c) { ::putc(c); }
