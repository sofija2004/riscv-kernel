#ifndef KSEMAPHORE_HPP
#define KSEMAPHORE_HPP

#include "KThread.hpp"

// Kernelska predstava semafora (Z3).
//
// Dizajn:
//  - vrednost je uvek nenegativna; niti koje ne mogu da prodju cekaju
//    u FIFO redu i svaka nosi svoj broj trazenih jedinica (need);
//  - wait/signal su specijalni slucajevi wait_n/signal_n sa n = 1,
//    pa jezgro implementira samo opsti mehanizam;
//  - politika budjenja je strogi FIFO: budi se s pocetka reda dok god
//    prvi cekalac moze da se zadovolji - nema izgladnjivanja velikih
//    zahteva preskakanjem;
//  - zivi semafor nosi tag (ista ideja kao kod alokatora), pa se
//    operacije nad zatvorenim/losim ruckama najcesce mogu detektovati.
class KSemaphore {
public:
    static KSemaphore* open(unsigned init);

    int wait(unsigned n);    // 0 / ERR_CLOSED (semafor zatvoren u toku cekanja)
    int signal(unsigned n);
    int close();             // budi SVE cekaoce sa greskom, dealocira

    static bool valid(KSemaphore* s);

    static constexpr int ERR_CLOSED     = -1;
    static constexpr int ERR_BAD_HANDLE = -2;

private:
    KSemaphore(unsigned init);

    uint64 tag;              // TAG_LIVE dok je semafor ziv
    unsigned value;
    KThread* qHead;          // FIFO red blokiranih niti
    KThread* qTail;

    static constexpr uint64 TAG_LIVE = 0x5E3AF04E;

    void enqueue(KThread* t);
    KThread* dequeue();
    void wakeHead(int status); // skida glavu reda i vraca je medju spremne
};

#endif // KSEMAPHORE_HPP
