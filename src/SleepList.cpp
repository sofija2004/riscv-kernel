#include "../h/SleepList.hpp"
#include "../h/Scheduler.hpp"

KThread* SleepList::head = nullptr;

void SleepList::put(KThread* t, unsigned long ticks) {
    // Nadji mesto: umanjuj preostalo za delte prethodnika.
    KThread** link = &head;
    while (*link && (*link)->sleepDelta <= ticks) {
        ticks -= (*link)->sleepDelta;
        link = &(*link)->next;
    }
    t->sleepDelta = ticks;
    t->next = *link;
    if (t->next) t->next->sleepDelta -= ticks; // sledbenik je sad relativan na nas
    *link = t;
}

void SleepList::tick() {
    if (!head) return;
    if (head->sleepDelta > 0) head->sleepDelta--;
    while (head && head->sleepDelta == 0) {
        KThread* t = head;
        head = t->next;
        t->next = nullptr;
        t->blockStatus = 0;
        t->state = KThread::State::READY;
        Scheduler::put(t);
    }
}
