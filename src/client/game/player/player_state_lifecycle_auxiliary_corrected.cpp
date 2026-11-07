#include "game/player/player_state_containers.h"
#include "runtime/allocations.h"

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
