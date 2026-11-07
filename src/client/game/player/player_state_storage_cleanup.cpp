#include "game/player/player_state_containers.h"
#include "runtime/allocations.h"

// The node-erase and range-erase routines below are byte-identical copies of
// the core/tree24 instantiations (same 24-byte node, header and count
// layout); the original simply kept one copy per map type. Each copy here
// forwards to the shared implementation.
extern "C" void THISCALL FUN_0042bd60(OriginalTree12* self, OriginalTreeNode24* node) {
    FUN_0041d3f0(self, node);
}
extern "C" void THISCALL FUN_0042bda0(OriginalTree12* self, OriginalTreeNode24* node) {
    FUN_0041d3f0(self, node);
}
extern "C" void THISCALL FUN_0042bde0(OriginalTree12* self, OriginalTreeNode24* node) {
    FUN_0041d3f0(self, node);
}
extern "C" void THISCALL FUN_0048d720(OriginalTree12* self, OriginalTreeNode24* node) {
    FUN_0041d3f0(self, node);
}
extern "C" void THISCALL FUN_004ce7c0(OriginalTree12* self, OriginalTreeNode24* node) {
    FUN_0041d3f0(self, node);
}
// Range erases: identical to 0x0041ee30 apart from calling the copies above
// and the 0x00422290 / 0x004d2bd0 copies of 0x0041e730.
extern "C" OriginalTreeNode24** THISCALL FUN_004d2b10(OriginalTree12* self,
                                                      OriginalTreeNode24** output,
                                                      OriginalTreeNode24* first,
                                                      OriginalTreeNode24* last) {
    return FUN_0041ee30(self, output, first, last);
}
extern "C" OriginalTreeNode24** THISCALL FUN_004d3a30(OriginalTree12* self,
                                                      OriginalTreeNode24** output,
                                                      OriginalTreeNode24* first,
                                                      OriginalTreeNode24* last) {
    return FUN_0041ee30(self, output, first, last);
}
extern "C" OriginalTreeNode24** THISCALL FUN_004d3af0(OriginalTree12* self,
                                                      OriginalTreeNode24** output,
                                                      OriginalTreeNode24* first,
                                                      OriginalTreeNode24* last) {
    return FUN_0041ee30(self, output, first, last);
}
extern "C" OriginalTreeNode24** THISCALL FUN_004d3bb0(OriginalTree12* self,
                                                      OriginalTreeNode24** output,
                                                      OriginalTreeNode24* first,
                                                      OriginalTreeNode24* last) {
    return FUN_0041ee30(self, output, first, last);
}
extern "C" OriginalTreeNode24** THISCALL FUN_004d3c70(OriginalTree12* self,
                                                      OriginalTreeNode24** output,
                                                      OriginalTreeNode24* first,
                                                      OriginalTreeNode24* last) {
    return FUN_0041ee30(self, output, first, last);
}

namespace {
void delete_member(void* object, uint32_t offset) {
    auto*& member = *reinterpret_cast<void**>(static_cast<uint8_t*>(object) + offset);
    if (member != nullptr) {
        fun_005cc620(member);
        member = nullptr;
    }
}
} // namespace

// Element destructors for the arrays released by 0x004ce3e0.
extern "C" void THISCALL FUN_00460b00(void* self) {
    delete_member(self, 0);
    delete_member(self, 4);
}
extern "C" void THISCALL FUN_00460b60(void* self) {
    delete_member(self, 0);
}
extern "C" void THISCALL FUN_00460ba0(void* self) {
    delete_member(self, 4);
    delete_member(self, 0x84);
}
extern "C" void THISCALL FUN_00460c00(void* self) {
    delete_member(self, 0);
    delete_member(self, 4);
    delete_member(self, 0x90);
}
extern "C" void THISCALL FUN_00460c50(void* self) {
    delete_member(self, 4);
    for (uint32_t index = 0; index != 13; ++index)
        delete_member(self, 0x3c + index * 4);
}
// 0x0047a7c0: element destructor for the three records at +0x1c of 0x0047a900.
extern "C" void THISCALL FUN_0047a7c0(void* self) {
    auto*& member = *reinterpret_cast<void**>(static_cast<uint8_t*>(self) + 0x10);
    if (member != nullptr) {
        fun_005cdc59(member);
        member = nullptr;
    }
}

// EH handler of the original 0x0042c870, which survives as an instruction
// listing in network/session_helper_instructions.cpp.
__asm__(".text\n"
        ".intel_syntax noprefix\n"
        ".globl _FUN_006ac528\n"
        "_FUN_006ac528:\n"
        "mov eax, offset _dat_006fb1d4\n"
        "jmp _fun_005cca04\n"
        ".att_syntax prefix\n");
