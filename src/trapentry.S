# trapentry.S - ulazna tacka svih supervizorskih prekida i izuzetaka.
#
# Jedan stek po niti: nema posebnog "sistemskog" steka. Prekidna
# rutina zato nikad ne menja stek - samo produzi na vec aktivnom sp,
# bilo da prekid dolazi iz U ili iz S rezima. Jednostavnije, uz cenu
# da se korisnicki i kernel-deo obrade te niti oslanjaju na isti,
# unapred odvojen prostor.
#
# Okvir (34 slota po 8 B = 272 B, poravnato na 16):
#   frame[0]      = sepc
#   frame[1..31]  = x1..x31 (frame[2] = sp prekinutog toka)
#   frame[32]     = sstatus
# sepc i sstatus su deo okvira jer se pri sinhronoj promeni konteksta
# unutar obrade druga nit vraca kroz SVOJ okvir - CSR-ovi se moraju
# restaurirati po niti, ne globalno.

.equ FRAME, 272

.global supervisorTrap
.align 4
supervisorTrap:
    addi sp, sp, -FRAME
    .irp reg, 1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
    sd x\reg, 8 * \reg (sp)
    .endr
    addi t0, sp, FRAME         # originalni sp
    sd t0, 8 * 2 (sp)

    csrr t0, sepc
    sd t0, 0 (sp)
    csrr t0, sstatus
    sd t0, 8 * 32 (sp)

    mv a0, sp
    call handleSupervisorTrap

    ld t0, 0 (sp)
    csrw sepc, t0
    ld t0, 8 * 32 (sp)
    csrw sstatus, t0

    .irp reg, 1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
    ld x\reg, 8 * \reg (sp)
    .endr
    ld sp, 8 * 2 (sp)          # poslednje: sp prekinutog toka
    sret
