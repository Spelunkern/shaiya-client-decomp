#include "game/player/player_state_bytes.h"
#include "game/player/player_state_containers.h"
#include "network/session_triplet_vector.h"
#include "runtime/allocations.h"

using namespace player_state_bytes;

namespace {
// Initial value of each of the eight default entries pushed by the reset.
constexpr uint16_t default_entry_codes[8] = {0x0002, 0x0101, 0x0201, 0x0006,
                                             0x0301, 0x0000, 0x0003, 0x0401};
// The pointer-vector owners and maps cleared by the reset, in its order.
OriginalPointerVectorOwner* vector_at(void* self, uint32_t offset) {
    return static_cast<OriginalPointerVectorOwner*>(at(self, offset));
}
OriginalTreeOwner* tree_at(void* self, uint32_t offset) {
    return static_cast<OriginalTreeOwner*>(at(self, offset));
}
// Element count as the original computes it: byte distance, arithmetic shift.
int32_t words_between(const uint32_t* first, const uint32_t* last) {
    return int32_t(reinterpret_cast<uint32_t>(last) - reinterpret_cast<uint32_t>(first)) >> 2;
}
} // namespace

// 0x004d4100: return the player state to its initial values.
extern "C" void THISCALL FUN_004d4100(void* self) {
    if (get32(self, 0x1999080) != 0) {
        fun_005cdc59(reinterpret_cast<void*>(get32(self, 0x1999080)));
        set32(self, 0x1999080, 0);
    }
    set32(self, 0x1999104, 0);
    set8(self, 0x199907e, 0xff);
    zero32(self, 0x6000, 0x960);
    zero32(self, 0x199977c, 0x960);
    set8(self, 0x199bcfc, 0);
    FUN_0042c7f0(tree_at(self, 0x199bd00));
    FUN_0042c680(vector_at(self, 0x199bd14));
    set32(self, 0x199bd44, 0);
    set8(self, 0x199bdbc, 0);
    set32(self, 0x199bdb0, 0);
    set32(self, 0x199bdb8, 0);
    set32(self, 0x199bdc0, 0);

    // The vector of default entries at +0x199bd30 is refilled when empty.
    auto* entries = static_cast<OriginalSessionPointerVector*>(at(self, 0x199bd30));
    if (entries->begin == nullptr || words_between(entries->begin, entries->end) < 1) {
        for (uint32_t index = 0; index != 8; ++index) {
            auto* entry = static_cast<uint8_t*>(fun_005cc96d(4));
            entry[2] = 0;
            entry[3] = 0;
            set16(entry, 0, default_entry_codes[index]);
            uint32_t value = reinterpret_cast<uint32_t>(entry);
            uint32_t* begin = entries->begin;
            const uint32_t size = begin == nullptr ? 0 : uint32_t(words_between(begin, entries->end));
            if (begin != nullptr && size < uint32_t(words_between(begin, entries->capacity_end)))
                *entries->end++ = value;
            else
                fun_00570310(entries, reinterpret_cast<uint32_t>(entries->end), 1,
                             reinterpret_cast<uint32_t>(&value));
        }
    }

    zero32(self, 0x19982f0, 0xc8);
    zero32(self, 0x10f4, 0x5a0);
    zero32(self, 0x199bdcc, 0x168);
    zero32(self, 0x1998614, 0x60);
    set32(self, 0x1998794, 0);
    for (uint32_t offset = 0x1998798; offset != 0x19987a8; offset += 4)
        set32(self, offset, 3);
    zero32(self, 0x19987a8, 8);
    zero32(self, 0xdc, 0x400);
    zero32(self, 0x27ac, 0x400);
    set8(self, 0x1998610, 0);
    set8(self, 0x1998611, 1);
    for (uint32_t offset = 8; offset != 0x2c; offset += 4)
        set32(self, offset, 0);
    for (uint32_t offset = 0x2c; offset != 0x3e; offset += 2)
        set16(self, offset, 0);
    for (uint32_t offset = 0x40; offset != 0x64; offset += 4)
        set32(self, offset, 0);
    set32(self, 0x84, 0);
    set32(self, 0x80, 0);
    set32(self, 0x7c, 0);
    set16(self, 0xa0, 0);
    set16(self, 0xa2, 0);
    set8(self, 0xa4, 0);
    set8(self, 0xa5, 0);
    set8(self, 0xa6, 0);
    set16(self, 0xa8, 0);
    set16(self, 0xaa, 0);
    set32(self, 0x10f0, 0);
    set16(self, 0x10ec, 0);
    set16(self, 0x10dc, 0);
    set32(self, 0x10e8, 0);
    set32(self, 0x10e4, 0);
    set32(self, 0x10e0, 0);
    set32(self, 0x27a0, 0);
    set32(self, 0x37f0, 0);
    set32(self, 0x37f4, 0);
    for (uint32_t offset = 0x881c; offset != 0x8838; offset += 4)
        set32(self, offset, 0);
    set8(self, 0x8838, 0);
    set16(self, 0x883a, 0);
    set8(self, 0x883c, 0);
    set16(self, 0x883e, 0);
    set8(self, 0x8840, 0);
    set8(self, 0x8841, 0);
    set32(self, 0x8844, 0);
    set32(self, 0x19982d0, 0);
    set8(self, 0x19982d4, 0);
    for (uint32_t offset = 0x19982d8; offset != 0x19982e8; offset += 4)
        set32(self, offset, 0);
    for (uint32_t offset = 0x884c; offset != 0x8868; offset += 4)
        set32(self, offset, 0);
    set8(self, 0x199817c, 0);
    set32(self, 0x19982e8, 0);
    set8(self, 0x19982ec, 3);
    set8(self, 0x19982ed, 2);
    set8(self, 0x19982ef, 0);
    set8(self, 0x19987c8, 0);
    set8(self, 0x19987c9, 0);
    set8(self, 0x19987ca, 0);
    set8(self, 0x19987cb, 0);
    set16(self, 0x19987cc, 0);
    set8(self, 0x8580, 0);
    set8(self, 0x199817d, 0);
    for (uint32_t offset = 0x19987dc; offset != 0x19987f0; offset += 4)
        set32(self, offset, 0);
    zero32(self, 0x1998868, 0x14);
    zero32(self, 0x19988b8, 0x14);
    for (uint32_t offset = 0x199890c; offset != 0x1998924; offset += 4)
        set32(self, offset, 0);

    FUN_004ce8a0(static_cast<OriginalListOwner*>(at(self, 0x19987f0)));
    FUN_0042ca80(vector_at(self, 0x19981dc));
    FUN_0048df40(tree_at(self, 0x19981f4));
    FUN_0042c680(vector_at(self, 0x19981b0));
    FUN_0042c680(vector_at(self, 0x1998198));
    FUN_0042c680(vector_at(self, 0x1998180));
    FUN_0042c6f0(tree_at(self, 0x19981c8));
    FUN_004d20f0(tree_at(self, 0x199822c));
    FUN_0042c680(vector_at(self, 0x1998240));
    FUN_0042c680(vector_at(self, 0x1998258));
    FUN_0042c680(vector_at(self, 0x1998270));
    FUN_0042c680(vector_at(self, 0x1998288));
    FUN_0042c680(vector_at(self, 0x19982a0));
    FUN_0042c770(tree_at(self, 0x19982b8));

    set8(self, 0x199c618, 0);
    set16(self, 0x1999008, 0);
    set16(self, 0x199900a, 0);
    set32(self, 0x1999010, 0);
    set32(self, 0x199900c, 0);
    set32(self, 0x1999018, 0xffffffff);
    set32(self, 0x1999014, 0xffffffff);
    for (uint32_t offset = 0x1999038; offset != 0x199904c; offset += 4)
        set32(self, offset, 0);
    zero32(self, 0x199904c, 0xc);
    set16(self, 0x199907c, 0);
    set8(self, 0x199914c, 0);
    set8(self, 0x199914d, 0);
    zero32(self, 0x199914e, 0xb4);
    zero32(self, 0x199941e, 0xb4);
    set32(self, 0x19996f0, 0);
    set8(self, 0x199973a, 1);
    set8(self, 0x19996f4, 1);
    set8(self, 0x1999738, 0);
    set8(self, 0x1999739, 0);
    set32(self, 0x199973b, 0);
    set16(self, 0x199973f, 0);
    FUN_004ce920(static_cast<OriginalListOwner*>(at(self, 0x1999760)));

    set32(self, 0x199c378, 0);
    set16(self, 0x199c37c, 0);
    set8(self, 0x199c37e, 0);
    set8(self, 0x199c37f, 0);
    set8(self, 0x199c380, 2);
    set32(self, 0x199c384, 0);
    set32(self, 0x1998934, 0);
    set32(self, 0x1998930, 0);
    set8(self, 0x1998b40, 0);
    set32(self, 0x1998b44, 0);
    set8(self, 0x1998b48, 0);
    set32(self, 0x1998b4c, 0xffffffff);
    set32(self, 0x1998b50, 0xffffffff);
    set32(self, 0x1999744, 0);
    zero32(self, 0x199c3d8, 0x80);
    for (uint32_t offset = 0x199c5d8; offset != 0x199c604; offset += 4)
        set32(self, offset, 0);
    FUN_0042c680(vector_at(self, 0x199c3c0));

    set32(self, 0x199c61c, 0xffffffff);
    set8(self, 0x199c620, 0xff);
    // The three-word vector at +0x199c624: free its storage and clear it.
    if (get32(self, 0x199c628) != 0)
        fun_005cc620(reinterpret_cast<void*>(get32(self, 0x199c628)));
    set32(self, 0x199c628, 0);
    set32(self, 0x199c62c, 0);
    set32(self, 0x199c630, 0);
}
