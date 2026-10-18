#ifndef KTHREAD_HPP
#define KTHREAD_HPP

#include "../lib/hw.h"

using uint64 = unsigned long;

// Kernelska predstava niti (Z2).
//
// Dizajn:
//  - kontekst cuva SAMO callee-saved skup (ra, sp, s0..s11); caller-saved
//    registre preko poziva contextSwitch stiti sama C konvencija, a
//    registri prekinutog korisnickog toka vec leze u okviru prekidne
//    rutine na steku niti;
//  - svaka nit ima JEDAN stek, koji koristi i za svoj korisnicki kod i
//    za obradu sopstvenih prekida/sistemskih poziva (nema posebnog
//    sistemskog steka) - jednostavnije, jer je sistem jednoprocesorski
//    i uvek se izvrsava tacno jedna nit;
//  - stanje niti je enum masina stanja, ne skup bool flegova;
//  - korisnicke niti izvrsavaju telo u U-rezimu; main, idle i interna
//    konzolna nit izvrsavaju se u privilegovanom S-rezimu.
class KThread {
public:
    using Body = void (*)(void*);

    enum class State { READY, RUNNING, BLOCKED, SLEEPING, FINISHED };

    // Pravi nit nad telom body sa argumentom arg; userStackTop je vrh
    // vec alociranog korisnickog steka (ABI ugovor iz speca 2026).
    // Nit odmah postaje READY i ulazi u red spremnih.
    static KThread* create(Body body, void* arg, void* userStackTop,
                           bool privileged = false);

    // Pravi internu nit jezgra i za nju alocira stek. Interna nit se
    // ne racuna medju niti korisnicke aplikacije i pocinje u S-rezimu.
    static KThread* createKernelThread(Body body, void* arg = nullptr);

    // Umotava tekuci tok (main) u nit, da bi mogao da bude ravnopravno
    // rasporedjivan. Nema sopstveno alociran stek: koristi zatecheni
    // (boot stek iz hw.lib).
    static KThread* wrapMain();

    // Sinhrona promena konteksta. requeue: da li tekucu nit vratiti u
    // red spremnih (thread_dispatch) ili ne (zavrsetak/blokiranje niti).
    static void dispatch(bool requeue);

    // Zavrsava tekucu nit; ne vraca se.
    static void exitRunning();

    // Blokira tekucu nit: nit je vec ulancana u red cekalaca nekog
    // semafora; vraca se tek kada je neko probudi, a povratnu vrednost
    // (0 ili kod greske) budilac ostavlja u blockStatus.
    static int blockRunning();

    // Uspavljuje tekucu nit na ticks otkucaja (Z4); vraca 0.
    static int sleepRunning(unsigned long ticks);

    // Otkucaj tajmera: azurira uspavane i trosi odsecak vremena tekuce
    // niti; po isteku odsecka vrsi ASINHRONU promenu konteksta.
    static void tick();

    // Program sme da se ugasi tek kada se zavrse sve korisnicke niti,
    // ukljucujuci niti koje je userMain napravio pre svog zavrsetka.
    static bool hasLiveUserThreads();

    static bool runningIsPrivileged();

    static KThread* running;

    struct Context {
        uint64 ra;
        uint64 sp;
        uint64 s[12];
    };

private:
    KThread(Body body, void* arg, void* userStackTop, bool privileged);

    Body body;
    void* arg;
    void* userStackTop;  // vrh jedinog steka (nullptr samo za umotani main)
    void* stackBase;     // baza istog steka; oslobadja se odlozeno
    Context ctx;
    State state;
    bool privileged;
    KThread* next;       // intruzivno ulancavanje: nit je ILI u redu
                         // spremnih ILI u redu cekalaca jednog semafora
                         // (stanja se iskljucuju), pa je pokazivac deljen
    unsigned need;       // trazene jedinice dok ceka na semaforu
    int blockStatus;     // povratna vrednost wait-a, upisuje je budilac
    unsigned long sleepDelta; // otkucaji POSLE prethodnika u delta listi

    static unsigned long sliceElapsed; // otkucaji od poslednje promene
    static unsigned liveUserThreads;

    friend class Scheduler;
    friend class KSemaphore;
    friend class SleepList;

    // Prva instrukcija svake nove niti (jos u S-rezimu).
    static void threadEntry();

    // Odlozena reciklaza zavrsene niti: oslobadja je PRVA nit koja se
    // izvrsi posle promene konteksta, jer se sam contextSwitch jos
    // oslanja na ctx unutar OVOG objekta dok se prebacivanje zavrsava -
    // nit ne sme da oslobodi taj prostor dok jos moze biti u upotrebi.
    static KThread* pendingReap;
    static void reapPending();
};

// Asemblerske rutine (switch.S).
extern "C" void contextSwitch(KThread::Context* oldCtx, KThread::Context* newCtx);
extern "C" void userEntry(uint64 body, uint64 arg, uint64 userStackTop);

#endif // KTHREAD_HPP
