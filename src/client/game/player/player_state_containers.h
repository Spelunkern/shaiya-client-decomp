#pragma once
#include <stdint.h>
#include <stddef.h>
#include "core/tree24.h"
#include "runtime/array_lifetime.h"
#include "runtime/file_ops.h"

// Containers owned by the player state. Each layout below is the one the
// original instructions address; members the owners never touch stay opaque.

// Polymorphic owner of a std::vector of pointers (vtable, allocator, range).
struct OriginalPointerVectorOwner {
    const void* vtable;
    uint32_t allocator;
    void** first;
    void** last;
    void** end;
};
static_assert(sizeof(OriginalPointerVectorOwner) == 0x14 &&
              offsetof(OriginalPointerVectorOwner, first) == 8);

// Polymorphic owner of a std::map with 24-byte nodes. The owners walk the map
// through their own cursor member at +0x10 rather than a local iterator.
struct OriginalTreeOwner {
    const void* vtable;
    OriginalTree12 tree;
    OriginalTreeNode24* cursor;
};
static_assert(sizeof(OriginalTreeOwner) == 0x14 && offsetof(OriginalTreeOwner, tree) == 4 &&
              offsetof(OriginalTreeOwner, cursor) == 0x10);

// std::list node and its polymorphic owner, with the same cursor member.
struct OriginalListNode {
    OriginalListNode* next;
    OriginalListNode* previous;
    void* value;
};
struct OriginalListOwner {
    const void* vtable;
    uint32_t allocator;
    OriginalListNode* head;
    uint32_t count;
    OriginalListNode* cursor;
};
static_assert(sizeof(OriginalListOwner) == 0x14 && offsetof(OriginalListOwner, head) == 8 &&
              offsetof(OriginalListOwner, cursor) == 0x10);

// Owner of a std::map whose destructor clears it through another owner first.
struct OriginalTreeHolder {
    const void* vtable;
    OriginalTree12 tree;
};
static_assert(sizeof(OriginalTreeHolder) == 0x10 && offsetof(OriginalTreeHolder, tree) == 4);

#define THISCALL __attribute__((thiscall))
extern "C" {
void fun_005cdc59(void* pointer);
}
// Destroys an array allocated with new[]: its element count sits in the
// cookie just before the first element, and the cookie is what gets freed.
inline void original_delete_array(void* first, uint32_t element_size,
                                  OriginalElementCallback destructor) {
    auto* cookie = static_cast<int32_t*>(first) - 1;
    fun_005cdcbc(first, element_size, *cookie, destructor);
    fun_005cdc59(cookie);
}
extern "C" {
// Virtual tables: one deleting-destructor slot each.
extern const void* const dat_006be5cc[1];
extern const void* const dat_006c1c58[1];
extern const void* const dat_006c2bc0[1];
extern const void* const dat_006c2be4[1];
extern const void* const dat_006c2be8[1];
extern const void* const dat_006c2bec[1];
extern const void* const dat_006c2bf0[1];
extern const void* const dat_006c2bf4[1];
extern const void* const dat_006c2bf8[1];
extern const void* const dat_006c2bfc[1];
extern const void* const dat_006c2c00[1];
extern const void* const dat_006c2c04[1];
extern const void* const dat_006c2c08[1];
extern const void* const dat_006c2c0c[1];
extern const void* const dat_006c2c10[1];
extern const void* const dat_006c2c14[1];
extern const void* const dat_006c2c18[1];
extern const void* const dat_006c2c1c[1];
extern const void* const dat_006c2c20[1];
extern const void* const dat_006c2ca4[1];
extern const void* const dat_006c2ca8[1];
extern const void* const dat_006c2cac[1];
extern const void* const dat_006c2cb0[1];
extern const void* const dat_006c2cb4[1];
extern const void* const dat_006c2cb8[1];

extern uint32_t dat_0224e650;

// External dependencies.
void THISCALL FUN_004ce800(void* list);

// Pointer vectors.
void THISCALL FUN_0042c680(OriginalPointerVectorOwner* self);
void THISCALL FUN_0042ca80(OriginalPointerVectorOwner* self);
void THISCALL FUN_0042c9e0(OriginalPointerVectorOwner* self);
void THISCALL FUN_0042ca40(void* self);

// Map clears through the cursor member, one per node-erase copy.
void THISCALL FUN_0042c6f0(OriginalTreeOwner* self);
void THISCALL FUN_0042c770(OriginalTreeOwner* self);
void THISCALL FUN_0042c7f0(OriginalTreeOwner* self);
void THISCALL FUN_004d20f0(OriginalTreeOwner* self);
void THISCALL FUN_0048df40(OriginalTreeOwner* self);

// Copies of the 24-byte tree routines in core/tree24.
void THISCALL FUN_0042bd60(OriginalTree12* self, OriginalTreeNode24* node);
void THISCALL FUN_0042bda0(OriginalTree12* self, OriginalTreeNode24* node);
void THISCALL FUN_0042bde0(OriginalTree12* self, OriginalTreeNode24* node);
void THISCALL FUN_0048d720(OriginalTree12* self, OriginalTreeNode24* node);
void THISCALL FUN_004ce7c0(OriginalTree12* self, OriginalTreeNode24* node);
OriginalTreeNode24** THISCALL FUN_00422290(OriginalTree12* self, OriginalTreeNode24** output,
                                           OriginalTreeNode24* target);
OriginalTreeNode24** THISCALL FUN_004d2bd0(OriginalTree12* self, OriginalTreeNode24** output,
                                           OriginalTreeNode24* target);
OriginalTreeNode24** THISCALL FUN_004d2b10(OriginalTree12* self, OriginalTreeNode24** output,
                                           OriginalTreeNode24* first, OriginalTreeNode24* last);
OriginalTreeNode24** THISCALL FUN_004d3a30(OriginalTree12* self, OriginalTreeNode24** output,
                                           OriginalTreeNode24* first, OriginalTreeNode24* last);
OriginalTreeNode24** THISCALL FUN_004d3af0(OriginalTree12* self, OriginalTreeNode24** output,
                                           OriginalTreeNode24* first, OriginalTreeNode24* last);
OriginalTreeNode24** THISCALL FUN_004d3bb0(OriginalTree12* self, OriginalTreeNode24** output,
                                           OriginalTreeNode24* first, OriginalTreeNode24* last);
OriginalTreeNode24** THISCALL FUN_004d3c70(OriginalTree12* self, OriginalTreeNode24** output,
                                           OriginalTreeNode24* first, OriginalTreeNode24* last);
void THISCALL FUN_004d3f00(OriginalTree12* self);
void THISCALL FUN_004d4040(OriginalTree12* self);
void THISCALL FUN_004d4070(OriginalTree12* self);
void THISCALL FUN_004d40a0(OriginalTree12* self);
void THISCALL FUN_004d40d0(OriginalTree12* self);

// Lists.
void THISCALL FUN_004ce8a0(OriginalListOwner* self);
void THISCALL FUN_004ce920(OriginalListOwner* self);

// Element destructors used by array new/delete.
void THISCALL FUN_00460b00(void* self);
void THISCALL FUN_00460b60(void* self);
void THISCALL FUN_00460ba0(void* self);
void THISCALL FUN_00460c00(void* self);
void THISCALL FUN_00460c50(void* self);
void THISCALL FUN_0047a7c0(void* self);
void THISCALL FUN_0047a810(void* self);
void THISCALL FUN_0047a880(void* self);
void THISCALL FUN_0047a900(void* self);
void THISCALL FUN_0047a990(void* self);
void THISCALL FUN_0047a9f0(void* self);
void THISCALL FUN_004ce3e0(void* self);
void THISCALL FUN_004ce530(void* self);
void* THISCALL FUN_005b3240(void* self);

// Map-owner destructors (vtable, clear, then the map itself).
void THISCALL FUN_004d3f50(OriginalTreeOwner* self);
void THISCALL FUN_004d59b0(OriginalTreeOwner* self);
void THISCALL FUN_004d5a30(OriginalTreeOwner* self);
void THISCALL FUN_004d5ab0(OriginalTreeOwner* self);
void THISCALL FUN_004d5b30(OriginalTreeOwner* self);
void THISCALL FUN_004d3ea0(OriginalPointerVectorOwner* self);

// Polymorphic object without members of its own.
void* THISCALL FUN_004a76f0(void* self);
void THISCALL FUN_004a7710(void* self);

// Player-state lifetime.
void* THISCALL FUN_004d5c30(void* self);
void THISCALL FUN_004d66a0(void* self);
void THISCALL FUN_004d4100(void* self);
void* THISCALL FUN_004cc0d0(void* self);
void* THISCALL FUN_004ce570(void* self);
void THISCALL FUN_004ce610(void* self);
void THISCALL FUN_004ce860(void* self);
uint32_t THISCALL FUN_00555f30(void* self);

bool THISCALL FUN_0059fe40(const int32_t* descriptor, void* buffer, uint32_t size);
bool THISCALL FUN_005b3250(void** self, const int32_t* descriptor);
}
