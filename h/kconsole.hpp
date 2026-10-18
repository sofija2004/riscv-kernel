#ifndef KCONSOLE_HPP
#define KCONSOLE_HPP

// Minimalni direktni izlaz samo za poruke samog jezgra pre pokretanja
// rasporedjivaca i za panic. Korisnicki getc/putc ne koriste ovaj modul,
// vec baferisanu klasu KernelConsole.

namespace kconsole {
    void putc(char c);
    bool rxReady();
    char getcBlocking();
    void puts(const char* s);
    void putHex(unsigned long v);
}

#endif
