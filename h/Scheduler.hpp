#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

class KThread;

// FIFO red spremnih niti, intruzivno ulancan kroz KThread::next.
// put i get su O(1) zahvaljujuci pokazivacima na glavu i rep.
class Scheduler {
public:
    static void put(KThread* t);
    static KThread* get();      // nullptr ako je red prazan
    static bool hasReady() { return head != nullptr; }

private:
    static KThread* head;
    static KThread* tail;
};

#endif // SCHEDULER_HPP
