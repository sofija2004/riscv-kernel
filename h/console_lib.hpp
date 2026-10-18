#ifndef CONSOLE_LIB_HPP
#define CONSOLE_LIB_HPP
// Inicijalizacija hardvera koju daje hw.lib (xv6).
extern "C" void uartinit();
extern "C" void consoleinit();
extern "C" void plicinit();
extern "C" void plicinithart();
// Nizak nivo: bajt iz UART-a ili -1; potvrda PLIC prekida.
extern "C" int  uartgetc();
extern "C" int  plic_claim(void);
extern "C" void plic_complete(int irq);
#endif
