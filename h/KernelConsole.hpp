#ifndef KERNEL_CONSOLE_HPP
#define KERNEL_CONSOLE_HPP

#include "../lib/hw.h"

class KSemaphore;
class KThread;

// Baferisana konzola jezgra.
//
// Put ulaza:
//   UART prekid -> direktno upisuje u inputBuffer -> getc
//
// Put izlaza:
//   putc -> outputBuffer -> privilegovana TX nit -> UART
//
// Ulaz i izlaz namerno NISU simetricni. Izlazna strana mora da preziveli
// vise budicih prekida (UART salje jedan bajt po bajt, uz cekanje) - zato
// joj treba sopstvena, trajna nit koja spava izmedju prekida i budi se na
// svaki sledeci. Ulazna strana zavrsi ceo svoj posao (pokupi sve dostupne
// znakove, upise ih, probudi cekaoca) unutar JEDNOG poziva prekidne
// rutine - nema sta da "preživljava" do sledeceg prekida, pa joj posebna
// nit ne bi nista donela sem dodatnog sloja. Prekidna rutina zato nikad
// ne ceka: ako je ulazni bafer pun, novi znak se prosto odbacuje (spec to
// izricito dozvoljava), dok se korisnicke niti blokiraju na praznom
// ulaznom ili punom izlaznom baferu.
//
// Zahvaljujuci tome ulaznom baferu pristupaju samo dva mesta i oba se
// uvek izvrsavaju sa vec maskiranim prekidima (sama prekidna rutina, i
// getc unutar obrade sistemskog poziva) - nije mu potrebno posebno
// zakljucavanje. Izlazni bafer, nasuprot tome, dira i TX nit (obican tok,
// prekidi joj nisu maskirani), pa njemu zakljucavanje ostaje neophodno.
class KernelConsole {
public:
    static bool initialize();

    static int getc();
    static void putc(char c);

    // Poziva se iz obrade spoljasnjeg prekida, dok su prekidi maskirani.
    static void handleInterrupt();

    // main koristi ovo pre gasenja da poslednji ispis ne ostane u baferu.
    static bool outputDrained();

private:
    static constexpr unsigned BUFFER_CAPACITY = 256;

    static char inputBuffer[BUFFER_CAPACITY];
    static unsigned inputHead, inputTail, inputCount;

    static char outputBuffer[BUFFER_CAPACITY];
    static unsigned outputHead, outputTail, outputCount;
    static bool outputBusy;

    static KSemaphore* inputItems;
    static KSemaphore* outputItems;
    static KSemaphore* outputSpace;
    static KSemaphore* txEvents;

    static KThread* txThread;
    static bool initialized;

    static void txBody(void*);

    static unsigned long lockInterrupts();
    static void restoreInterrupts(unsigned long savedStatus);
};

#endif // KERNEL_CONSOLE_HPP
