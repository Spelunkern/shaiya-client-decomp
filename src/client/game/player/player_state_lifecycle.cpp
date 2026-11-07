#include "game/player/player_state_bytes.h"
#include "game/player/player_state_containers.h"

using namespace player_state_bytes;

// 0x004cc0d0: two blocks of 0x7c00 bytes, each counted by a leading dword.
extern "C" void* THISCALL FUN_004cc0d0(void* self) {
    for (uint32_t block = 0; block != 2; ++block) {
        set32(self, block * 4, 0);
        zero32(self, 8 + block * 0x7c00, 0x1f00);
    }
    return self;
}

// 0x004ce530: destructor of the four 8-byte records at +0x198008c of the
// 0x004ce570 object: release the new[] array of 12-byte entries at +4.
extern "C" void THISCALL FUN_004ce530(void* self) {
    if (get32(self, 4) != 0) {
        original_delete_array(reinterpret_cast<void*>(get32(self, 4)), 0xc,
                              reinterpret_cast<OriginalElementCallback>(FUN_0047a9f0));
        set32(self, 4, 0);
    }
}

// 0x004ce570: constructor of the player-state object at +0x88c8. It holds
// 0x10000 records of 0x198 bytes whose two 200-byte blocks are cleared (the
// four bytes after each block stay untouched), then the four 8-byte records,
// then null array pointers.
extern "C" void* THISCALL FUN_004ce570(void* self) {
    set_pointer(self, 0, dat_006c2be4);
    for (uint32_t record = 0; record != 0x10000; ++record) {
        zero32(self, 0x90 + record * 0x198, 0x32);
        zero32(self, 0x15c + record * 0x198, 0x32);
    }
    fun_005ce6bb(at(self, 0x198008c), 8, 4, reinterpret_cast<OriginalElementCallback>(FUN_005b3240),
                 reinterpret_cast<OriginalElementCallback>(FUN_004ce530));
    set32(self, 4, 0);
    set32(self, 0xc, 0);
    set32(self, 0x84, 0);
    set32(self, 8, 0);
    set32(self, 0x10, 0);
    set32(self, 0x88, 0);
    for (uint32_t index = 0; index != 14; ++index) {
        set32(self, 0x14 + index * 4, 0);
        set32(self, 0x4c + index * 4, 0);
    }
    return self;
}
