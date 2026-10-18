#ifndef RISCV_HPP
#define RISCV_HPP

// Pomocne funkcije za rad sa kontrolno-statusnim registrima (CSR)
// supervizorskog rezima procesora RISC-V, kao inline asm omotaci.

using uint64 = unsigned long;

namespace riscv {

    // Uzroci sinhronih izuzetaka (registar scause).
    constexpr uint64 CAUSE_ILLEGAL = 2;  // ilegalna instrukcija (privilegija)
    constexpr uint64 CAUSE_ECALL_U = 8;  // ecall iz korisnickog rezima
    constexpr uint64 CAUSE_ECALL_S = 9;  // ecall iz sistemskog rezima

    // Prekidi: najvisi bit scause je 1, kod u ostalim bitima.
    constexpr uint64 CAUSE_INTERRUPT  = 1UL << 63;
    constexpr uint64 CAUSE_SOFTWARE_S = CAUSE_INTERRUPT | 1; // tajmerski otkucaj
    constexpr uint64 CAUSE_EXTERNAL_S = CAUSE_INTERRUPT | 9; // spoljasnji (konzola)

    // Bitovi sip/sie registara.
    constexpr uint64 SIP_SSIP = 1UL << 1;
    constexpr uint64 SIE_SSIE = 1UL << 1;

    // Bitovi registra sstatus.
    constexpr uint64 SSTATUS_SPP  = 1UL << 8; // rezim pre prekida (1=S, 0=U)
    constexpr uint64 SSTATUS_SPIE = 1UL << 5; // SIE pre prekida
    constexpr uint64 SSTATUS_SIE  = 1UL << 1; // maska prekida

    inline uint64 r_scause() {
        uint64 v; asm volatile("csrr %0, scause" : "=r"(v)); return v;
    }
    inline uint64 r_sepc() {
        uint64 v; asm volatile("csrr %0, sepc" : "=r"(v)); return v;
    }
    inline void w_sepc(uint64 v) { asm volatile("csrw sepc, %0" : : "r"(v)); }
    inline uint64 r_stval() {
        uint64 v; asm volatile("csrr %0, stval" : "=r"(v)); return v;
    }
    inline void w_stvec(uint64 v) { asm volatile("csrw stvec, %0" : : "r"(v)); }
    inline uint64 r_sstatus() {
        uint64 v; asm volatile("csrr %0, sstatus" : "=r"(v)); return v;
    }
    inline void w_sstatus(uint64 v) { asm volatile("csrw sstatus, %0" : : "r"(v)); }
    inline void mc_sip(uint64 mask) { asm volatile("csrc sip, %0" : : "r"(mask)); }
    inline void ms_sie(uint64 mask) { asm volatile("csrs sie, %0" : : "r"(mask)); }
    inline void ms_sstatus(uint64 mask) { asm volatile("csrs sstatus, %0" : : "r"(mask)); }
    inline void mc_sstatus(uint64 mask) { asm volatile("csrc sstatus, %0" : : "r"(mask)); }

} // namespace riscv

// Ulazna tacka prekidne rutine, definisana u trapentry.S.
extern "C" void supervisorTrap();

// C++ deo obrade: prima pokazivac na okvir sacuvanih registara.
// Raspored okvira (vidi trapentry.S): frame[0]=sepc, frame[i]=x_i (1..31),
// frame[32]=sstatus. Povratna vrednost sistemskog poziva upisuje se
// u frame[10] (a0).
extern "C" void handleSupervisorTrap(uint64* frame);

#endif // RISCV_HPP
