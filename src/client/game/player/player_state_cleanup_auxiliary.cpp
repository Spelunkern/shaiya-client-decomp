#include "game/player/player_state_containers.h"
#include "runtime/allocations.h"

namespace {
// Map destructor: erase every node through the given range-erase copy, then
// free the header and clear the count.
template <OriginalTreeNode24** THISCALL (*EraseRange)(OriginalTree12*, OriginalTreeNode24**,
                                                      OriginalTreeNode24*, OriginalTreeNode24*)>
void destroy_tree(OriginalTree12* self) {
    OriginalTreeNode24* ignored;
    EraseRange(self, &ignored, self->header->left, self->header);
    fun_005cc620(self->header);
    self->header = nullptr;
    self->count = 0;
}
} // namespace

extern "C" void THISCALL FUN_004d3f00(OriginalTree12* self) {
    destroy_tree<FUN_004d2b10>(self);
}
extern "C" void THISCALL FUN_004d4040(OriginalTree12* self) {
    destroy_tree<FUN_004d3a30>(self);
}
extern "C" void THISCALL FUN_004d4070(OriginalTree12* self) {
    destroy_tree<FUN_004d3af0>(self);
}
extern "C" void THISCALL FUN_004d40a0(OriginalTree12* self) {
    destroy_tree<FUN_004d3bb0>(self);
}
extern "C" void THISCALL FUN_004d40d0(OriginalTree12* self) {
    destroy_tree<FUN_004d3c70>(self);
}

// Deleting destructors (virtual slot 0): destroy, then free when bit 0 of
// the flags is set.
extern "C" void* THISCALL FUN_0042ca20(OriginalPointerVectorOwner* self, uint32_t flags) {
    FUN_0042c9e0(self);
    if (flags & 1)
        fun_005cc620(self);
    return self;
}
extern "C" void* THISCALL FUN_004a7720(void* self, uint32_t flags) {
    *static_cast<const void**>(self) = dat_006c1c58;
    if (flags & 1)
        fun_005cc620(self);
    return self;
}

// Unwind funclet of the original 0x0042c870, which survives as an instruction
// listing in network/session_helper_instructions.cpp: destroys its temporary
// string.
__asm__(".text\n"
        ".intel_syntax noprefix\n"
        ".globl _FUN_006ac520\n"
        "_FUN_006ac520:\n"
        "lea ecx, [ebp - 0x28]\n"
        "jmp _FUN_00529db0\n"
        ".att_syntax prefix\n");
