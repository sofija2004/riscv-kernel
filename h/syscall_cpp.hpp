#ifndef SYSCALL_CPP_HPP
#define SYSCALL_CPP_HPP

#include "syscall_c.h"

void* operator new(size_t);
void operator delete(void*) noexcept;
void* operator new[](size_t);
void operator delete[](void*) noexcept;

class Thread {
public:
    Thread(void (*body)(void*), void* arg);
    virtual ~Thread();
    int start();
    static void dispatch();
    static int sleep(time_t);
protected:
    Thread();
    virtual void run() {}
private:
    thread_t myHandle;
    void (*body)(void*);
    void* arg;
    static void runHelper(void* self); // starter za run() varijantu
};

class Semaphore {
public:
    Semaphore(unsigned init = 1);
    virtual ~Semaphore();
    int wait();
    int signal();
private:
    sem_t myHandle;
};

class PeriodicThread : public Thread {
public:
    void terminate();
protected:
    PeriodicThread(time_t period);
    virtual void periodicActivation() {}
    virtual void run() override; // petlja aktivacija+sleep (Z4)
private:
    time_t period; // 0 posle terminate() - i signal za stop i period
};

class Console {
public:
    static char getc();
    static void putc(char);
};

#endif // SYSCALL_CPP_HPP
