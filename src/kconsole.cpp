#include "../h/kconsole.hpp"

// UART 16550 na QEMU virt masini.
static volatile unsigned char* const UART = (unsigned char*)0x10000000UL;
static constexpr int RBR = 0, THR = 0, LSR = 5;
static constexpr unsigned char LSR_RX_READY = 0x01, LSR_TX_EMPTY = 0x20;

namespace kconsole {

    void putc(char c) {
        while (!(UART[LSR] & LSR_TX_EMPTY)) {}
        UART[THR] = (unsigned char)c;
    }

    bool rxReady() {
        return (UART[LSR] & LSR_RX_READY) != 0;
    }

    char getcBlocking() {
        while (!(UART[LSR] & LSR_RX_READY)) {}
        return (char)UART[RBR];
    }

    void puts(const char* s) {
        while (*s) putc(*s++);
    }

    void putHex(unsigned long v) {
        puts("0x");
        for (int i = 60; i >= 0; i -= 4)
            putc("0123456789ABCDEF"[(v >> i) & 0xF]);
    }
}
