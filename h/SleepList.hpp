#ifndef SLEEPLIST_HPP
#define SLEEPLIST_HPP

#include "KThread.hpp"

// Red uspavanih niti kao DELTA LISTA: svaka nit pamti koliko otkucaja
// spava POSLE svog prethodnika, pa otkucaj tajmera umanjuje samo
// brojac glave liste - O(1) po otkucaju, nezavisno od broja spavaca.
class SleepList {
public:
    static void put(KThread* t, unsigned long ticks); // ticks > 0
    static void tick(); // jedan otkucaj: budi sve dospele
private:
    static KThread* head;
};

#endif // SLEEPLIST_HPP
