# contextSwitch(Context* stari, Context* novi)
# Cuva callee-saved skup (ra, sp, s0..s11) starog konteksta i ucitava
# novi. Caller-saved registre stiti sama C konvencija poziva: pozivalac
# contextSwitch-a ih je vec sacuvao ako mu trebaju. Registri prekinutog
# korisnickog programa su vec u okviru prekidne rutine na steku niti,
# koji se ovim sp-om takodje "nosi".

.global contextSwitch
.type contextSwitch, @function
contextSwitch:
    sd ra, 0(a0)
    sd sp, 8(a0)
    .irp n, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
    sd s\n, 16 + 8 * \n (a0)
    .endr

    ld ra, 0(a1)
    ld sp, 8(a1)
    .irp n, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
    ld s\n, 16 + 8 * \n (a1)
    .endr
    ret

# userEntry(body, arg, userStackTop)
# Poslednji korak pokretanja korisnicke niti: postavlja korisnicki
# stek i sret-om prelazi u U-rezim (sepc i sstatus su vec postavljeni
# u KThread::threadEntry). body i arg ostaju u a0 i a1 kao argumenti
# funkcije userThreadWrapper na koju sepc pokazuje.
.global userEntry
.type userEntry, @function
userEntry:
    mv sp, a2
    sret
