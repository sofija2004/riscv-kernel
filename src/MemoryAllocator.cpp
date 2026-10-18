#include "../h/MemoryAllocator.hpp"
#include "../h/Riscv.hpp"

MemoryAllocator::FreeSeg* MemoryAllocator::head = nullptr;
bool MemoryAllocator::initialized = false;

// Lista slobodnih segmenata je deljeno stanje kojem mogu da pristupe i
// obicne niti (kroz sistemski poziv, iz prekidne rutine) i interne
// privilegovane niti direktno (npr. idle nit koja samo diskutuje sa
// rasporedjivacem). Zato svaka javna operacija sama maskira prekide
// oko svog kriticnog dela, umesto da to prepusti pozivaocu.
static inline uint64 lockInterrupts() {
    uint64 saved = riscv::r_sstatus();
    riscv::mc_sstatus(riscv::SSTATUS_SIE);
    return saved;
}

static inline void restoreInterrupts(uint64 saved) {
    if (saved & riscv::SSTATUS_SIE)
        riscv::ms_sstatus(riscv::SSTATUS_SIE);
}

// Lenja inicijalizacija: pri prvom pozivu ceo hip postaje jedan
// slobodan segment. Ne oslanjamo se na globalne konstruktore jer
// u jezgru njihov redosled (i samo izvrsavanje) nije garantovano.
void MemoryAllocator::initOnFirstUse() {
    if (initialized) return;
    initialized = true;

    char* start = (char*)HEAP_START_ADDR;
    char* end   = (char*)HEAP_END_ADDR;
    size_t totalBlocks = (size_t)(end - start) / MEM_BLOCK_SIZE;
    if (totalBlocks < 2) { head = nullptr; return; }

    head = (FreeSeg*)start;
    head->blocks = totalBlocks;
    head->next = nullptr;
}

void* MemoryAllocator::alloc(size_t userBlocks) {
    uint64 savedStatus = lockInterrupts();

    initOnFirstUse();
    if (userBlocks == 0) { restoreInterrupts(savedStatus); return nullptr; }

    size_t needed = userBlocks + 1; // +1 blok za zaglavlje

    // Best-fit: najmanji slobodan segment koji je >= needed.
    FreeSeg* best = nullptr;
    FreeSeg* bestPrev = nullptr;
    FreeSeg* prev = nullptr;
    for (FreeSeg* cur = head; cur; prev = cur, cur = cur->next) {
        if (cur->blocks >= needed && (!best || cur->blocks < best->blocks)) {
            best = cur;
            bestPrev = prev;
        }
    }
    if (!best) { restoreInterrupts(savedStatus); return nullptr; }

    if (best->blocks == needed) {
        // Tacan pogodak: vadimo ceo segment iz liste.
        if (bestPrev) bestPrev->next = best->next;
        else head = best->next;
    } else {
        // Secenje: korisnik dobija pocetak, ostatak nasledjuje
        // mesto starog segmenta u listi (sortiranost ocuvana,
        // jer ostatak i dalje prethodi best->next po adresi).
        FreeSeg* rest = (FreeSeg*)((char*)best + needed * MEM_BLOCK_SIZE);
        rest->blocks = best->blocks - needed;
        rest->next = best->next;
        if (bestPrev) bestPrev->next = rest;
        else head = rest;
    }

    UsedHdr* hdr = (UsedHdr*)best;
    hdr->blocks = needed;
    hdr->tag = TAG_USED;

    restoreInterrupts(savedStatus);
    return (char*)hdr + MEM_BLOCK_SIZE; // prvi korisnicki blok
}

int MemoryAllocator::release(void* addr) {
    uint64 savedStatus = lockInterrupts();

    initOnFirstUse();
    char* p      = (char*)addr;
    char* hStart = (char*)HEAP_START_ADDR;
    char* hEnd   = (char*)HEAP_END_ADDR;

    // Validacija opsega i poravnanja: korisnicka adresa je drugi
    // blok segmenta, pa zaglavlje ne moze biti pre pocetka hipa.
    if (p < hStart + MEM_BLOCK_SIZE || p >= hEnd) {
        restoreInterrupts(savedStatus);
        return ERR_OUT_OF_RANGE;
    }
    if ((size_t)(p - hStart) % MEM_BLOCK_SIZE != 0) {
        restoreInterrupts(savedStatus);
        return ERR_OUT_OF_RANGE;
    }

    UsedHdr* hdr = (UsedHdr*)(p - MEM_BLOCK_SIZE);
    if (hdr->tag != TAG_USED) { restoreInterrupts(savedStatus); return ERR_BAD_TAG; } // dupli free / los pokazivac
    size_t blocks = hdr->blocks;
    if (blocks < 2 || (char*)hdr + blocks * MEM_BLOCK_SIZE > hEnd) {
        restoreInterrupts(savedStatus);
        return ERR_BAD_TAG; // korumpirano zaglavlje
    }
    hdr->tag = 0; // segment vise nije ziv

    FreeSeg* seg = (FreeSeg*)hdr;
    seg->blocks = blocks;

    // Ubacivanje na pravo mesto u sortiranoj listi (I1).
    FreeSeg* prev = nullptr;
    FreeSeg* cur = head;
    while (cur && cur < seg) { prev = cur; cur = cur->next; }
    seg->next = cur;
    if (prev) prev->next = seg;
    else head = seg;

    // Spajanje odmah (I2): prvo desni sused, pa levi, ovim redom,
    // da bi seg ostao validan dok proveravamo oba dodirivanja.
    if (cur && (char*)seg + seg->blocks * MEM_BLOCK_SIZE == (char*)cur) {
        seg->blocks += cur->blocks;
        seg->next = cur->next;
    }
    if (prev && (char*)prev + prev->blocks * MEM_BLOCK_SIZE == (char*)seg) {
        prev->blocks += seg->blocks;
        prev->next = seg->next;
    }

    restoreInterrupts(savedStatus);
    return 0;
}
