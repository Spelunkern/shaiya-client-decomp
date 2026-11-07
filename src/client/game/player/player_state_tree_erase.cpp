#include "game/player/player_state_containers.h"

// Both node erases are byte-identical to 0x0041e730 except for the EH handler
// that destroys the "invalid map/set<T> iterator" message on unwind; that
// cleanup is part of the shared implementation.
extern "C" OriginalTreeNode24** THISCALL FUN_00422290(OriginalTree12* self,
                                                      OriginalTreeNode24** output,
                                                      OriginalTreeNode24* target) {
    return FUN_0041e730(self, output, target);
}
extern "C" OriginalTreeNode24** THISCALL FUN_004d2bd0(OriginalTree12* self,
                                                      OriginalTreeNode24** output,
                                                      OriginalTreeNode24* target) {
    return FUN_0041e730(self, output, target);
}

// 0x0059fe40: _read size bytes from the descriptor into buffer; true when the
// whole block was read.
extern "C" bool THISCALL FUN_0059fe40(const int32_t* descriptor, void* buffer, uint32_t size) {
    return uint32_t(fun_005ce53d(*descriptor, buffer, size)) == size;
}
