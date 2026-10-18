#ifndef SYSCALL_C_H
#define SYSCALL_C_H

#include "../lib/hw.h"

// C API sloj interfejsa jezgra (spec 2026, tabela sistemskih poziva).
// Svaka funkcija pakuje argumente po ABI konvenciji (a0 = kod poziva,
// a1.. = argumenti) i izvrsava instrukciju ecall.

// --- Z1: memorija ---
void* mem_alloc(size_t size);   // size u BAJTOVIMA; zaokruzuje na blokove
int mem_free(void* ptr);

// --- Z2: niti ---
class _thread;
typedef _thread* thread_t;

int thread_create(thread_t* handle, void (*start_routine)(void*), void* arg);
int thread_exit();
void thread_dispatch();

// --- Z3: semafori ---
class _sem;
typedef _sem* sem_t;

int sem_open(sem_t* handle, unsigned init);
int sem_close(sem_t handle);
int sem_wait(sem_t id);
int sem_signal(sem_t id);
int sem_wait_n(sem_t id, unsigned n);
int sem_signal_n(sem_t id, unsigned n);

// --- Z4: vreme i konzola ---
typedef unsigned long time_t;
int time_sleep(time_t t);

const int EOF = -1;
char getc();
void putc(char c);

#endif // SYSCALL_C_H
