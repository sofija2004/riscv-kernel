#include "../h/Scheduler.hpp"
#include "../h/KThread.hpp"

KThread* Scheduler::head = nullptr;
KThread* Scheduler::tail = nullptr;

void Scheduler::put(KThread* t) {
    t->next = nullptr;
    if (tail) tail->next = t;
    else head = t;
    tail = t;
}

KThread* Scheduler::get() {
    KThread* t = head;
    if (!t) return nullptr;
    head = t->next;
    if (!head) tail = nullptr;
    t->next = nullptr;
    return t;
}
