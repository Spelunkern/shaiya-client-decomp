#include "game/player/player_state_containers.h"
#include "runtime/allocations.h"

// Unwind funclets of 0x004ce610 and 0x004d5c30, still instruction listings.
__asm__(".text\n"
        ".intel_syntax noprefix\n"
        ".globl _FUN_006b0f30\n"
        "_FUN_006b0f30:\n"
        ".globl _player_lifecycle_aux_location_006b0f30\n"
        "_player_lifecycle_aux_location_006b0f30:\n"
        ".att_syntax prefix\npushl $_FUN_004ce530\n.intel_syntax noprefix\n"
        ".globl _player_lifecycle_aux_location_006b0f35\n"
        "_player_lifecycle_aux_location_006b0f35:\n"
        "push 4\n"
        ".globl _player_lifecycle_aux_location_006b0f37\n"
        "_player_lifecycle_aux_location_006b0f37:\n"
        "push 8\n"
        ".globl _player_lifecycle_aux_location_006b0f39\n"
        "_player_lifecycle_aux_location_006b0f39:\n"
        "mov eax, dword ptr [ebp - 0x10]\n"
        ".globl _player_lifecycle_aux_location_006b0f3c\n"
        "_player_lifecycle_aux_location_006b0f3c:\n"
        "add eax, 0x198008c\n"
        ".globl _player_lifecycle_aux_location_006b0f41\n"
        "_player_lifecycle_aux_location_006b0f41:\n"
        "push eax\n"
        ".globl _player_lifecycle_aux_location_006b0f42\n"
        "_player_lifecycle_aux_location_006b0f42:\n"
        "call \"_fun_005cdcbc@16\"\n"
        ".globl _player_lifecycle_aux_location_006b0f47\n"
        "_player_lifecycle_aux_location_006b0f47:\n"
        "ret\n"
        ".globl _FUN_006b1040\n"
        "_FUN_006b1040:\n"
        ".globl _player_lifecycle_aux_location_006b1040\n"
        "_player_lifecycle_aux_location_006b1040:\n"
        "mov ecx, dword ptr [ebp - 0x10]\n"
        ".globl _player_lifecycle_aux_location_006b1043\n"
        "_player_lifecycle_aux_location_006b1043:\n"
        "add ecx, 0x8868\n"
        ".globl _player_lifecycle_aux_location_006b1049\n"
        "_player_lifecycle_aux_location_006b1049:\n"
        "jmp _FUN_004ce860\n"
        ".globl _FUN_006b104e\n"
        "_FUN_006b104e:\n"
        ".globl _player_lifecycle_aux_location_006b104e\n"
        "_player_lifecycle_aux_location_006b104e:\n"
        "mov ecx, dword ptr [ebp - 0x10]\n"
        ".globl _player_lifecycle_aux_location_006b1051\n"
        "_player_lifecycle_aux_location_006b1051:\n"
        "add ecx, 0x8898\n"
        ".globl _player_lifecycle_aux_location_006b1057\n"
        "_player_lifecycle_aux_location_006b1057:\n"
        "jmp _FUN_004ce860\n"
        ".globl _FUN_006b105c\n"
        "_FUN_006b105c:\n"
        ".globl _player_lifecycle_aux_location_006b105c\n"
        "_player_lifecycle_aux_location_006b105c:\n"
        "mov ecx, dword ptr [ebp - 0x10]\n"
        ".globl _player_lifecycle_aux_location_006b105f\n"
        "_player_lifecycle_aux_location_006b105f:\n"
        "add ecx, 0x88c8\n"
        ".globl _player_lifecycle_aux_location_006b1065\n"
        "_player_lifecycle_aux_location_006b1065:\n"
        "jmp _FUN_004ce610\n"
        ".globl _FUN_006b106a\n"
        "_FUN_006b106a:\n"
        ".globl _player_lifecycle_aux_location_006b106a\n"
        "_player_lifecycle_aux_location_006b106a:\n"
        "mov ecx, dword ptr [ebp - 0x10]\n"
        ".globl _player_lifecycle_aux_location_006b106d\n"
        "_player_lifecycle_aux_location_006b106d:\n"
        "add ecx, 0x1998180\n"
        ".globl _player_lifecycle_aux_location_006b1073\n"
        "_player_lifecycle_aux_location_006b1073:\n"
        "jmp _FUN_004d2860\n"
        ".globl _FUN_006b1078\n"
        "_FUN_006b1078:\n"
        ".globl _player_lifecycle_aux_location_006b1078\n"
        "_player_lifecycle_aux_location_006b1078:\n"
        "mov ecx, dword ptr [ebp - 0x10]\n"
        ".globl _player_lifecycle_aux_location_006b107b\n"
        "_player_lifecycle_aux_location_006b107b:\n"
        "add ecx, 0x1998198\n"
        ".globl _player_lifecycle_aux_location_006b1081\n"
        "_player_lifecycle_aux_location_006b1081:\n"
        "jmp _FUN_004d28a0\n"
        ".globl _FUN_006b1086\n"
        "_FUN_006b1086:\n"
        ".globl _player_lifecycle_aux_location_006b1086\n"
        "_player_lifecycle_aux_location_006b1086:\n"
        "mov ecx, dword ptr [ebp - 0x10]\n"
        ".globl _player_lifecycle_aux_location_006b1089\n"
        "_player_lifecycle_aux_location_006b1089:\n"
        "add ecx, 0x19981b0\n"
        ".globl _player_lifecycle_aux_location_006b108f\n"
        "_player_lifecycle_aux_location_006b108f:\n"
        "jmp _FUN_004d28e0\n"
        ".globl _FUN_006b1094\n"
        "_FUN_006b1094:\n"
        ".globl _player_lifecycle_aux_location_006b1094\n"
        "_player_lifecycle_aux_location_006b1094:\n"
        "mov ecx, dword ptr [ebp - 0x10]\n"
        ".globl _player_lifecycle_aux_location_006b1097\n"
        "_player_lifecycle_aux_location_006b1097:\n"
        "add ecx, 0x19981c8\n"
        ".globl _player_lifecycle_aux_location_006b109d\n"
        "_player_lifecycle_aux_location_006b109d:\n"
        "jmp _FUN_004d3f50\n"
        ".globl _FUN_006b10a2\n"
        "_FUN_006b10a2:\n"
        ".globl _player_lifecycle_aux_location_006b10a2\n"
        "_player_lifecycle_aux_location_006b10a2:\n"
        "mov ecx, dword ptr [ebp - 0x10]\n"
        ".globl _player_lifecycle_aux_location_006b10a5\n"
        "_player_lifecycle_aux_location_006b10a5:\n"
        "add ecx, 0x19981dc\n"
        ".globl _player_lifecycle_aux_location_006b10ab\n"
        "_player_lifecycle_aux_location_006b10ab:\n"
        "jmp _FUN_004d3ea0\n"
        ".globl _FUN_006b10b0\n"
        "_FUN_006b10b0:\n"
        ".globl _player_lifecycle_aux_location_006b10b0\n"
        "_player_lifecycle_aux_location_006b10b0:\n"
        "mov ecx, dword ptr [ebp - 0x10]\n"
        ".globl _player_lifecycle_aux_location_006b10b3\n"
        "_player_lifecycle_aux_location_006b10b3:\n"
        "add ecx, 0x19981f4\n"
        ".globl _player_lifecycle_aux_location_006b10b9\n"
        "_player_lifecycle_aux_location_006b10b9:\n"
        "jmp _FUN_004d59b0\n"
        ".globl _FUN_006b10be\n"
        "_FUN_006b10be:\n"
        ".globl _player_lifecycle_aux_location_006b10be\n"
        "_player_lifecycle_aux_location_006b10be:\n"
        "mov ecx, dword ptr [ebp - 0x10]\n"
        ".globl _player_lifecycle_aux_location_006b10c1\n"
        "_player_lifecycle_aux_location_006b10c1:\n"
        "add ecx, 0x199822c\n"
        ".globl _player_lifecycle_aux_location_006b10c7\n"
        "_player_lifecycle_aux_location_006b10c7:\n"
        "jmp _FUN_004d5a30\n"
        ".globl _FUN_006b10cc\n"
        "_FUN_006b10cc:\n"
        ".globl _player_lifecycle_aux_location_006b10cc\n"
        "_player_lifecycle_aux_location_006b10cc:\n"
        "mov ecx, dword ptr [ebp - 0x10]\n"
        ".globl _player_lifecycle_aux_location_006b10cf\n"
        "_player_lifecycle_aux_location_006b10cf:\n"
        "add ecx, 0x1998240\n"
        ".globl _player_lifecycle_aux_location_006b10d5\n"
        "_player_lifecycle_aux_location_006b10d5:\n"
        "jmp _FUN_004d2170\n"
        ".globl _FUN_006b10da\n"
        "_FUN_006b10da:\n"
        ".globl _player_lifecycle_aux_location_006b10da\n"
        "_player_lifecycle_aux_location_006b10da:\n"
        "mov ecx, dword ptr [ebp - 0x10]\n"
        ".globl _player_lifecycle_aux_location_006b10dd\n"
        "_player_lifecycle_aux_location_006b10dd:\n"
        "add ecx, 0x1998258\n"
        ".globl _player_lifecycle_aux_location_006b10e3\n"
        "_player_lifecycle_aux_location_006b10e3:\n"
        "jmp _FUN_004d21b0\n"
        ".globl _FUN_006b10e8\n"
        "_FUN_006b10e8:\n"
        ".globl _player_lifecycle_aux_location_006b10e8\n"
        "_player_lifecycle_aux_location_006b10e8:\n"
        "mov ecx, dword ptr [ebp - 0x10]\n"
        ".globl _player_lifecycle_aux_location_006b10eb\n"
        "_player_lifecycle_aux_location_006b10eb:\n"
        "add ecx, 0x1998270\n"
        ".globl _player_lifecycle_aux_location_006b10f1\n"
        "_player_lifecycle_aux_location_006b10f1:\n"
        "jmp _FUN_004d21f0\n"
        ".globl _FUN_006b10f6\n"
        "_FUN_006b10f6:\n"
        ".globl _player_lifecycle_aux_location_006b10f6\n"
        "_player_lifecycle_aux_location_006b10f6:\n"
        "mov ecx, dword ptr [ebp - 0x10]\n"
        ".globl _player_lifecycle_aux_location_006b10f9\n"
        "_player_lifecycle_aux_location_006b10f9:\n"
        "add ecx, 0x1998288\n"
        ".globl _player_lifecycle_aux_location_006b10ff\n"
        "_player_lifecycle_aux_location_006b10ff:\n"
        "jmp _FUN_004d2230\n"
        ".globl _FUN_006b1104\n"
        "_FUN_006b1104:\n"
        ".globl _player_lifecycle_aux_location_006b1104\n"
        "_player_lifecycle_aux_location_006b1104:\n"
        "mov ecx, dword ptr [ebp - 0x10]\n"
        ".globl _player_lifecycle_aux_location_006b1107\n"
        "_player_lifecycle_aux_location_006b1107:\n"
        "add ecx, 0x19982a0\n"
        ".globl _player_lifecycle_aux_location_006b110d\n"
        "_player_lifecycle_aux_location_006b110d:\n"
        "jmp _FUN_004d2920\n"
        ".globl _FUN_006b1112\n"
        "_FUN_006b1112:\n"
        ".globl _player_lifecycle_aux_location_006b1112\n"
        "_player_lifecycle_aux_location_006b1112:\n"
        "mov ecx, dword ptr [ebp - 0x10]\n"
        ".globl _player_lifecycle_aux_location_006b1115\n"
        "_player_lifecycle_aux_location_006b1115:\n"
        "add ecx, 0x19982b8\n"
        ".globl _player_lifecycle_aux_location_006b111b\n"
        "_player_lifecycle_aux_location_006b111b:\n"
        "jmp _FUN_004d5ab0\n"
        ".globl _FUN_006b1120\n"
        "_FUN_006b1120:\n"
        ".globl _player_lifecycle_aux_location_006b1120\n"
        "_player_lifecycle_aux_location_006b1120:\n"
        "mov ecx, dword ptr [ebp - 0x10]\n"
        ".globl _player_lifecycle_aux_location_006b1123\n"
        "_player_lifecycle_aux_location_006b1123:\n"
        "add ecx, 0x19987f0\n"
        ".globl _player_lifecycle_aux_location_006b1129\n"
        "_player_lifecycle_aux_location_006b1129:\n"
        "jmp _FUN_004d0f70\n"
        ".globl _FUN_006b112e\n"
        "_FUN_006b112e:\n"
        ".globl _player_lifecycle_aux_location_006b112e\n"
        "_player_lifecycle_aux_location_006b112e:\n"
        "mov ecx, dword ptr [ebp - 0x10]\n"
        ".globl _player_lifecycle_aux_location_006b1131\n"
        "_player_lifecycle_aux_location_006b1131:\n"
        "add ecx, 0x1998804\n"
        ".globl _player_lifecycle_aux_location_006b1137\n"
        "_player_lifecycle_aux_location_006b1137:\n"
        "jmp _FUN_004d0fa0\n"
        ".globl _FUN_006b113c\n"
        "_FUN_006b113c:\n"
        ".globl _player_lifecycle_aux_location_006b113c\n"
        "_player_lifecycle_aux_location_006b113c:\n"
        "mov ecx, dword ptr [ebp - 0x10]\n"
        ".globl _player_lifecycle_aux_location_006b113f\n"
        "_player_lifecycle_aux_location_006b113f:\n"
        "add ecx, 0x199912c\n"
        ".globl _player_lifecycle_aux_location_006b1145\n"
        "_player_lifecycle_aux_location_006b1145:\n"
        "jmp _FUN_004d0fd0\n"
        ".globl _FUN_006b114a\n"
        "_FUN_006b114a:\n"
        ".globl _player_lifecycle_aux_location_006b114a\n"
        "_player_lifecycle_aux_location_006b114a:\n"
        "mov ecx, dword ptr [ebp - 0x10]\n"
        ".globl _player_lifecycle_aux_location_006b114d\n"
        "_player_lifecycle_aux_location_006b114d:\n"
        "add ecx, 0x1999760\n"
        ".globl _player_lifecycle_aux_location_006b1153\n"
        "_player_lifecycle_aux_location_006b1153:\n"
        "jmp _FUN_004d1000\n"
        ".globl _FUN_006b1158\n"
        "_FUN_006b1158:\n"
        ".globl _player_lifecycle_aux_location_006b1158\n"
        "_player_lifecycle_aux_location_006b1158:\n"
        "mov ecx, dword ptr [ebp - 0x10]\n"
        ".globl _player_lifecycle_aux_location_006b115b\n"
        "_player_lifecycle_aux_location_006b115b:\n"
        "add ecx, 0x199bd00\n"
        ".globl _player_lifecycle_aux_location_006b1161\n"
        "_player_lifecycle_aux_location_006b1161:\n"
        "jmp _FUN_004d5b30\n"
        ".globl _FUN_006b1166\n"
        "_FUN_006b1166:\n"
        ".globl _player_lifecycle_aux_location_006b1166\n"
        "_player_lifecycle_aux_location_006b1166:\n"
        "mov ecx, dword ptr [ebp - 0x10]\n"
        ".globl _player_lifecycle_aux_location_006b1169\n"
        "_player_lifecycle_aux_location_006b1169:\n"
        "add ecx, 0x199bd14\n"
        ".globl _player_lifecycle_aux_location_006b116f\n"
        "_player_lifecycle_aux_location_006b116f:\n"
        "jmp _FUN_004d2960\n"
        ".globl _FUN_006b1174\n"
        "_FUN_006b1174:\n"
        ".globl _player_lifecycle_aux_location_006b1174\n"
        "_player_lifecycle_aux_location_006b1174:\n"
        "mov ecx, dword ptr [ebp - 0x10]\n"
        ".globl _player_lifecycle_aux_location_006b1177\n"
        "_player_lifecycle_aux_location_006b1177:\n"
        "add ecx, 0x199bd2c\n"
        ".globl _player_lifecycle_aux_location_006b117d\n"
        "_player_lifecycle_aux_location_006b117d:\n"
        "jmp _FUN_004d29a0\n"
        ".globl _FUN_006b1182\n"
        "_FUN_006b1182:\n"
        ".globl _player_lifecycle_aux_location_006b1182\n"
        "_player_lifecycle_aux_location_006b1182:\n"
        "mov ecx, dword ptr [ebp - 0x10]\n"
        ".globl _player_lifecycle_aux_location_006b1185\n"
        "_player_lifecycle_aux_location_006b1185:\n"
        "add ecx, 0x199c36c\n"
        ".globl _player_lifecycle_aux_location_006b118b\n"
        "_player_lifecycle_aux_location_006b118b:\n"
        "jmp _FUN_004a7710\n"
        ".globl _FUN_006b1190\n"
        "_FUN_006b1190:\n"
        ".globl _player_lifecycle_aux_location_006b1190\n"
        "_player_lifecycle_aux_location_006b1190:\n"
        "mov ecx, dword ptr [ebp - 0x10]\n"
        ".globl _player_lifecycle_aux_location_006b1193\n"
        "_player_lifecycle_aux_location_006b1193:\n"
        "add ecx, 0x199c3c0\n"
        ".globl _player_lifecycle_aux_location_006b1199\n"
        "_player_lifecycle_aux_location_006b1199:\n"
        "jmp _FUN_004d2270\n"
        ".globl _FUN_006b119e\n"
        "_FUN_006b119e:\n"
        ".globl _player_lifecycle_aux_location_006b119e\n"
        "_player_lifecycle_aux_location_006b119e:\n"
        "mov ecx, dword ptr [ebp - 0x10]\n"
        ".globl _player_lifecycle_aux_location_006b11a1\n"
        "_player_lifecycle_aux_location_006b11a1:\n"
        "add ecx, 0x199c624\n"
        ".globl _player_lifecycle_aux_location_006b11a7\n"
        "_player_lifecycle_aux_location_006b11a7:\n"
        "jmp _FUN_00555f30\n"
        ".att_syntax prefix\n");

extern "C" {
void THISCALL FUN_004ce610(void* self);
void THISCALL FUN_004d0f70(void* self);
void THISCALL FUN_004d0fa0(void* self);
void THISCALL FUN_004d0fd0(void* self);
void THISCALL FUN_004d1000(void* self);
void THISCALL FUN_004d2170(void* self);
void THISCALL FUN_004d21b0(void* self);
void THISCALL FUN_004d21f0(void* self);
void THISCALL FUN_004d2230(void* self);
void THISCALL FUN_004d2270(void* self);
void THISCALL FUN_004d2860(void* self);
void THISCALL FUN_004d28a0(void* self);
void THISCALL FUN_004d28e0(void* self);
void THISCALL FUN_004d2920(void* self);
void THISCALL FUN_004d2960(void* self);
void THISCALL FUN_004d29a0(void* self);
}

namespace {
// Deleting destructor (virtual slot 0): destroy, then free when bit 0 of the
// flags is set.
template <class T>
void* destroy_and_delete(void* self, uint32_t flags, void THISCALL (*destroy)(T*)) {
    destroy(static_cast<T*>(self));
    if (flags & 1)
        fun_005cc620(self);
    return self;
}
} // namespace

extern "C" void* THISCALL FUN_004ce740(void* self, uint32_t flags) {
    return destroy_and_delete(self, flags, FUN_004ce610);
}
extern "C" void* THISCALL FUN_004d1030(void* self, uint32_t flags) {
    return destroy_and_delete(self, flags, FUN_004d0f70);
}
extern "C" void* THISCALL FUN_004d1050(void* self, uint32_t flags) {
    return destroy_and_delete(self, flags, FUN_004d0fa0);
}
extern "C" void* THISCALL FUN_004d1070(void* self, uint32_t flags) {
    return destroy_and_delete(self, flags, FUN_004d0fd0);
}
extern "C" void* THISCALL FUN_004d1090(void* self, uint32_t flags) {
    return destroy_and_delete(self, flags, FUN_004d1000);
}
extern "C" void* THISCALL FUN_004d22b0(void* self, uint32_t flags) {
    return destroy_and_delete(self, flags, FUN_004d2170);
}
extern "C" void* THISCALL FUN_004d22d0(void* self, uint32_t flags) {
    return destroy_and_delete(self, flags, FUN_004d21b0);
}
extern "C" void* THISCALL FUN_004d22f0(void* self, uint32_t flags) {
    return destroy_and_delete(self, flags, FUN_004d21f0);
}
extern "C" void* THISCALL FUN_004d2310(void* self, uint32_t flags) {
    return destroy_and_delete(self, flags, FUN_004d2230);
}
extern "C" void* THISCALL FUN_004d2330(void* self, uint32_t flags) {
    return destroy_and_delete(self, flags, FUN_004d2270);
}
extern "C" void* THISCALL FUN_004d29e0(void* self, uint32_t flags) {
    return destroy_and_delete(self, flags, FUN_004d2860);
}
extern "C" void* THISCALL FUN_004d2a00(void* self, uint32_t flags) {
    return destroy_and_delete(self, flags, FUN_004d28a0);
}
extern "C" void* THISCALL FUN_004d2a20(void* self, uint32_t flags) {
    return destroy_and_delete(self, flags, FUN_004d28e0);
}
extern "C" void* THISCALL FUN_004d2a40(void* self, uint32_t flags) {
    return destroy_and_delete(self, flags, FUN_004d2920);
}
extern "C" void* THISCALL FUN_004d2a60(void* self, uint32_t flags) {
    return destroy_and_delete(self, flags, FUN_004d2960);
}
extern "C" void* THISCALL FUN_004d2a80(void* self, uint32_t flags) {
    return destroy_and_delete(self, flags, FUN_004d29a0);
}
extern "C" void* THISCALL FUN_004d3f30(void* self, uint32_t flags) {
    return destroy_and_delete(self, flags, FUN_004d3ea0);
}
extern "C" void* THISCALL FUN_004d4020(void* self, uint32_t flags) {
    return destroy_and_delete(self, flags, FUN_004d3f50);
}
extern "C" void* THISCALL FUN_004d5bb0(void* self, uint32_t flags) {
    return destroy_and_delete(self, flags, FUN_004d59b0);
}
extern "C" void* THISCALL FUN_004d5bd0(void* self, uint32_t flags) {
    return destroy_and_delete(self, flags, FUN_004d5a30);
}
extern "C" void* THISCALL FUN_004d5bf0(void* self, uint32_t flags) {
    return destroy_and_delete(self, flags, FUN_004d5ab0);
}
extern "C" void* THISCALL FUN_004d5c10(void* self, uint32_t flags) {
    return destroy_and_delete(self, flags, FUN_004d5b30);
}
// 0x004ce870 inlines 0x004ce860: set the vtable, release the arrays.
extern "C" void* THISCALL FUN_004ce870(void* self, uint32_t flags) {
    *static_cast<const void**>(self) = dat_006c2bc0;
    FUN_004ce3e0(self);
    if (flags & 1)
        fun_005cc620(self);
    return self;
}
