#ifndef MEMORY_ALLOCATOR_HPP
#define MEMORY_ALLOCATOR_HPP

#include "../lib/hw.h"

// Alokator memorije jezgra (Z1).
//
// Strategija: best-fit nad jednostruko ulancanom listom slobodnih
// segmenata, sortiranom rastuce po adresi, uz spajanje (koalescenciju)
// fizicki susednih slobodnih segmenata odmah pri svakom oslobadjanju.
//
// Invarijante:
//  (I1) lista slobodnih segmenata je sortirana rastuce po adresi;
//  (I2) ne postoje dva fizicki susedna slobodna segmenta;
//  (I3) svaki zauzet segment pocinje blokom zaglavlja ciji tag ima
//       vrednost TAG_USED.
//
// Jedinica racunanja unutar klase je iskljucivo blok velicine
// MEM_BLOCK_SIZE; konverzija iz bajtova obavlja se u C API sloju.
//
// alloc/release mogu biti pozvani i iz konteksta gde prekidi NISU
// maskirani (npr. direktno iz tela interne niti, a ne kroz sistemski
// poziv), pa lista slobodnih segmenata sama sebe stiti maskiranjem
// prekida - ne oslanja se na to da je pozivalac to vec uradio.
class MemoryAllocator {
public:
    // Alocira segment od userBlocks blokova korisnickog prostora
    // (interno zauzima userBlocks + 1 blok zbog zaglavlja).
    // Vraca adresu prvog korisnickog bloka ili nullptr pri neuspehu.
    static void* alloc(size_t userBlocks);

    // Oslobadja segment prethodno dobijen iz alloc.
    // Vraca 0 pri uspehu, negativan kod greske pri neuspehu.
    static int release(void* addr);

    // Kodovi gresaka koje release moze da vrati.
    static constexpr int ERR_OUT_OF_RANGE = -1; // van hipa / lose poravnat
    static constexpr int ERR_BAD_TAG      = -2; // nije ziv segment (npr. dupli free)

private:
    // Zaglavlje slobodnog segmenta - zivi u prvim bajtovima samog
    // slobodnog prostora.
    struct FreeSeg {
        size_t blocks;   // ukupna velicina segmenta, u blokovima
        FreeSeg* next;   // sledeci slobodan segment, po rastucoj adresi
    };

    // Zaglavlje zauzetog segmenta - zauzima ceo prvi blok segmenta,
    // da bi korisnicki deo ostao poravnat na MEM_BLOCK_SIZE.
    struct UsedHdr {
        size_t blocks;   // ukupna velicina segmenta (sa zaglavljem), u blokovima
        size_t tag;      // TAG_USED dok je segment ziv
    };

    static constexpr size_t TAG_USED = 0xA10CA7ED;

    static FreeSeg* head;        // glava liste slobodnih segmenata
    static bool initialized;

    static void initOnFirstUse(); // ceo hip postaje jedan slobodan segment

    MemoryAllocator() = delete;   // cisto staticka klasa, bez instanci
};

#endif // MEMORY_ALLOCATOR_HPP
