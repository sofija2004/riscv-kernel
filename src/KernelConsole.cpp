#include "../h/KernelConsole.hpp"

#include "../h/KSemaphore.hpp"
#include "../h/KThread.hpp"
#include "../h/Riscv.hpp"
#include "../h/console_lib.hpp"

constexpr unsigned KernelConsole::BUFFER_CAPACITY;

char KernelConsole::inputBuffer[BUFFER_CAPACITY];
unsigned KernelConsole::inputHead = 0;
unsigned KernelConsole::inputTail = 0;
unsigned KernelConsole::inputCount = 0;

char KernelConsole::outputBuffer[BUFFER_CAPACITY];
unsigned KernelConsole::outputHead = 0;
unsigned KernelConsole::outputTail = 0;
unsigned KernelConsole::outputCount = 0;
bool KernelConsole::outputBusy = false;

KSemaphore* KernelConsole::inputItems = nullptr;
KSemaphore* KernelConsole::outputItems = nullptr;
KSemaphore* KernelConsole::outputSpace = nullptr;
KSemaphore* KernelConsole::txEvents = nullptr;

KThread* KernelConsole::txThread = nullptr;
bool KernelConsole::initialized = false;

unsigned long KernelConsole::lockInterrupts() {
    unsigned long saved = riscv::r_sstatus();
    riscv::mc_sstatus(riscv::SSTATUS_SIE);
    return saved;
}

void KernelConsole::restoreInterrupts(unsigned long savedStatus) {
    if (savedStatus & riscv::SSTATUS_SIE)
        riscv::ms_sstatus(riscv::SSTATUS_SIE);
}

bool KernelConsole::initialize() {
    if (initialized) return true;

    inputItems = KSemaphore::open(0);
    outputItems = KSemaphore::open(0);
    outputSpace = KSemaphore::open(BUFFER_CAPACITY);
    txEvents = KSemaphore::open(0);

    if (!inputItems || !outputItems || !outputSpace || !txEvents)
        return false;

    txThread = KThread::createKernelThread(&txBody);
    if (!txThread) return false;

    initialized = true;
    return true;
}

void KernelConsole::handleInterrupt() {
    if (!initialized) return;

    // Prekidna rutina je proizvodjac ulaznih znakova i ne sme da ceka,
    // pa se pun bafer resava jedino odbacivanjem (dozvoljeno postavkom).
    // Radi se direktno u pravi ulazni bafer - njemu, osim ove rutine,
    // pristupa samo getc, i oba mesta se uvek izvrsavaju sa vec
    // maskiranim prekidima, pa ovde nije potrebno posebno zakljucavanje.
    int value;
    while ((value = uartgetc()) >= 0) {
        if (inputCount < BUFFER_CAPACITY) {
            inputBuffer[inputTail] = (char)value;
            inputTail = (inputTail + 1) % BUFFER_CAPACITY;
            inputCount++;
            inputItems->signal(1);
        }
        // inace: bafer pun, znak se gubi
    }

    // Isti spoljasnji prekid moze znaciti i da je UART spreman za
    // naredni izlazni znak. Dogadjaj ostaje zapamcen u semaforu.
    txEvents->signal(1);
}

int KernelConsole::getc() {
    if (!initialized || inputItems->wait(1) < 0) return -1;

    char value = inputBuffer[inputHead];
    inputHead = (inputHead + 1) % BUFFER_CAPACITY;
    inputCount--;
    return (unsigned char)value;
}

void KernelConsole::putc(char c) {
    if (!initialized || outputSpace->wait(1) < 0) return;

    unsigned long saved = lockInterrupts();
    outputBuffer[outputTail] = c;
    outputTail = (outputTail + 1) % BUFFER_CAPACITY;
    outputCount++;
    restoreInterrupts(saved);

    outputItems->signal(1);
}

void KernelConsole::txBody(void*) {
    for (;;) {
        outputItems->wait(1);

        unsigned long saved = lockInterrupts();
        char value = outputBuffer[outputHead];
        outputHead = (outputHead + 1) % BUFFER_CAPACITY;
        outputCount--;
        outputBusy = true;
        restoreInterrupts(saved);
        outputSpace->signal(1);

        // Ceka hardverski dogadjaj, a ne vrti se u sistemskom pozivu.
        while (!(*(volatile unsigned char*)CONSOLE_STATUS &
                 CONSOLE_TX_STATUS_BIT)) {
            txEvents->wait(1);
        }

        *(volatile unsigned char*)CONSOLE_TX_DATA = (unsigned char)value;

        saved = lockInterrupts();
        outputBusy = false;
        restoreInterrupts(saved);
    }
}

bool KernelConsole::outputDrained() {
    unsigned long saved = lockInterrupts();
    bool drained = outputCount == 0 && !outputBusy;
    restoreInterrupts(saved);
    return drained;
}
