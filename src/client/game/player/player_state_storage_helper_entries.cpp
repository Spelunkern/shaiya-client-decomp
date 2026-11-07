#include "game/player/player_state_containers.h"
#include "runtime/allocations.h"

namespace {
// The originals compute the element count from the current range on every
// iteration (the deletes may not change it, but nothing is cached).
uint32_t pointer_count(const OriginalPointerVectorOwner* self) {
    return uint32_t(int32_t(reinterpret_cast<uint32_t>(self->last) -
                            reinterpret_cast<uint32_t>(self->first)) >>
                    2);
}
void release_pointer_storage(OriginalPointerVectorOwner* self) {
    if (self->first != nullptr)
        fun_005cc620(self->first);
    self->first = nullptr;
    self->last = nullptr;
    self->end = nullptr;
}
// Destructor body shared by the pointer-vector owners: set the vtable, delete
// every element, then release the storage.
void destroy_pointer_vector(OriginalPointerVectorOwner* self, const void* vtable) {
    self->vtable = vtable;
    FUN_0042c680(self);
    release_pointer_storage(self);
}

// Clear of a map<key, T*> owner: destroy each mapped value through the cursor
// member, then free every node with the given node-erase copy and reset the
// header to an empty tree. The count is cleared between the header links.
template <void THISCALL (*EraseNodes)(OriginalTree12*, OriginalTreeNode24*), class DestroyValue>
void clear_tree_owner(OriginalTreeOwner* self, DestroyValue destroy_value) {
    self->cursor = self->tree.header->left;
    while (self->cursor != self->tree.header) {
        destroy_value(reinterpret_cast<void*>(self->cursor->payload10));
        FUN_00421780(&self->cursor);
    }
    OriginalTreeNode24* node = self->tree.header->parent;
    while (node->is_header == 0) {
        EraseNodes(&self->tree, node->right);
        OriginalTreeNode24* current = node;
        node = node->left;
        fun_005cc620(current);
    }
    self->tree.header->parent = self->tree.header;
    self->tree.count = 0;
    self->tree.header->left = self->tree.header;
    self->tree.header->right = self->tree.header;
}
void delete_value(void* value) {
    if (value != nullptr)
        fun_005cc620(value);
}
void destroy_and_delete_value(void* value) {
    if (value != nullptr) {
        FUN_0042ca40(value);
        fun_005cc620(value);
    }
}

// Clear of a std::list owner: optionally destroy each value, then unlink and
// free every node. The value slot of each node is cleared after its delete.
template <class DestroyValue> void clear_list_owner(OriginalListOwner* self, DestroyValue destroy) {
    self->cursor = self->head->next;
    while (self->cursor != self->head) {
        void* value = self->cursor->value;
        if (value != nullptr) {
            destroy(value);
            fun_005cc620(value);
            self->cursor->value = nullptr;
        }
        self->cursor = self->cursor->next;
    }
    OriginalListNode* node = self->head->next;
    self->head->next = self->head;
    self->head->previous = self->head;
    self->count = 0;
    while (node != self->head) {
        OriginalListNode* next = node->next;
        fun_005cc620(node);
        node = next;
    }
}

// Owner of a list (vtable, list at +4): clear, tidy the list, free its head.
template <void THISCALL (*Clear)(OriginalListOwner*)>
void destroy_list_owner(OriginalListOwner* self, const void* vtable) {
    self->vtable = vtable;
    Clear(self);
    FUN_004ce800(&self->allocator);
    fun_005cc620(self->head);
    self->head = nullptr;
}

// Frees a heap pointer member and clears it.
void free_member(void* object, uint32_t offset) {
    auto*& member = *reinterpret_cast<void**>(static_cast<uint8_t*>(object) + offset);
    if (member != nullptr) {
        fun_005cdc59(member);
        member = nullptr;
    }
}
void delete_array(void* first, uint32_t element_size, OriginalElementCallback destructor) {
    original_delete_array(first, element_size, destructor);
}
void*& member_at(void* object, uint32_t offset) {
    return *reinterpret_cast<void**>(static_cast<uint8_t*>(object) + offset);
}
} // namespace

// 0x0042c680: delete every element of a pointer vector, clearing each slot,
// then release the storage.
extern "C" void THISCALL FUN_0042c680(OriginalPointerVectorOwner* self) {
    for (uint32_t index = 0; self->first != nullptr && index < pointer_count(self); ++index) {
        void* element = self->first[index];
        if (element != nullptr) {
            fun_005cc620(element);
            self->first[index] = nullptr;
        }
    }
    release_pointer_storage(self);
}
// 0x0042ca80: the same for a vector of 0x0042ca40 objects.
extern "C" void THISCALL FUN_0042ca80(OriginalPointerVectorOwner* self) {
    for (uint32_t index = 0; self->first != nullptr && index < pointer_count(self); ++index) {
        void* element = self->first[index];
        if (element != nullptr) {
            FUN_0042ca40(element);
            fun_005cc620(element);
            self->first[index] = nullptr;
        }
    }
    release_pointer_storage(self);
}

extern "C" void THISCALL FUN_0042c6f0(OriginalTreeOwner* self) {
    clear_tree_owner<FUN_0042bd60>(self, delete_value);
}
extern "C" void THISCALL FUN_0042c770(OriginalTreeOwner* self) {
    clear_tree_owner<FUN_0042bda0>(self, delete_value);
}
extern "C" void THISCALL FUN_0042c7f0(OriginalTreeOwner* self) {
    clear_tree_owner<FUN_0042bde0>(self, delete_value);
}
extern "C" void THISCALL FUN_004d20f0(OriginalTreeOwner* self) {
    clear_tree_owner<FUN_004ce7c0>(self, delete_value);
}
extern "C" void THISCALL FUN_0048df40(OriginalTreeOwner* self) {
    clear_tree_owner<FUN_0048d720>(self, destroy_and_delete_value);
}

// Element destructors: free heap members and clear them.
extern "C" void THISCALL FUN_0047a810(void* self) {
    free_member(self, 0x14);
    free_member(self, 0x18);
}
extern "C" void THISCALL FUN_0047a880(void* self) {
    free_member(self, 0x18);
    free_member(self, 0x1c);
}
extern "C" void THISCALL FUN_0047a900(void* self) {
    free_member(self, 0x14);
    free_member(self, 0x18);
    fun_005cdcbc(static_cast<uint8_t*>(self) + 0x1c, 0x18, 3,
                 reinterpret_cast<OriginalElementCallback>(FUN_0047a7c0));
}
extern "C" void THISCALL FUN_0047a990(void* self) {
    free_member(self, 4);
    free_member(self, 8);
    for (uint32_t index = 0; index != 7; ++index)
        free_member(self, 0xc8 + index * 4);
}
extern "C" void THISCALL FUN_0047a9f0(void* self) {
    free_member(self, 4);
    free_member(self, 8);
}

// 0x004a76f0 / 0x004a7710: constructor and destructor of a polymorphic
// object whose own members are two cleared words.
extern "C" void* THISCALL FUN_004a76f0(void* self) {
    member_at(self, 0) = const_cast<void**>(dat_006c1c58);
    member_at(self, 4) = nullptr;
    member_at(self, 8) = nullptr;
    return self;
}
extern "C" void THISCALL FUN_004a7710(void* self) {
    member_at(self, 0) = const_cast<void**>(dat_006c1c58);
}

// 0x004ce3e0: release the arrays of a player-state record. Every array was
// allocated with new[], so each one is destroyed element by element first.
extern "C" void THISCALL FUN_004ce3e0(void* self) {
    free_member(self, 8);
    // An array of new[] arrays; its length is the byte at +0x0c, re-read on
    // every iteration.
    for (int32_t index = 0; index < int32_t(*(static_cast<uint8_t*>(self) + 0xc)); ++index) {
        void*& entry = static_cast<void**>(member_at(self, 4))[index];
        if (entry != nullptr) {
            delete_array(entry, 0x5c, reinterpret_cast<OriginalElementCallback>(FUN_00460b00));
            static_cast<void**>(member_at(self, 4))[index] = nullptr;
        }
    }
    free_member(self, 4);
    if (member_at(self, 0x10) != nullptr) {
        delete_array(member_at(self, 0x10), 0x2c,
                     reinterpret_cast<OriginalElementCallback>(FUN_00460b60));
        member_at(self, 0x10) = nullptr;
    }
    if (member_at(self, 0x18) != nullptr) {
        for (uint32_t index = 0; index != 9; ++index) {
            void*& entry = static_cast<void**>(member_at(self, 0x18))[index];
            if (entry != nullptr) {
                delete_array(entry, 0x88, reinterpret_cast<OriginalElementCallback>(FUN_00460ba0));
                static_cast<void**>(member_at(self, 0x18))[index] = nullptr;
            }
        }
        free_member(self, 0x18);
    }
    if (member_at(self, 0x20) != nullptr) {
        delete_array(member_at(self, 0x20), 0x94,
                     reinterpret_cast<OriginalElementCallback>(FUN_00460c00));
        member_at(self, 0x20) = nullptr;
    }
    if (member_at(self, 0x28) != nullptr) {
        delete_array(member_at(self, 0x28), 0x70,
                     reinterpret_cast<OriginalElementCallback>(FUN_00460c50));
        member_at(self, 0x28) = nullptr;
    }
}
// 0x004ce860: set the record's vtable, then run 0x004ce3e0.
extern "C" void THISCALL FUN_004ce860(void* self) {
    member_at(self, 0) = const_cast<void**>(dat_006c2bc0);
    FUN_004ce3e0(self);
}

// 0x004ce8a0: clear a list of records, freeing each record's buffer at +0x24.
extern "C" void THISCALL FUN_004ce8a0(OriginalListOwner* self) {
    clear_list_owner(self, [](void* value) { free_member(value, 0x24); });
}
// 0x004ce920: clear a list of plain heap values.
extern "C" void THISCALL FUN_004ce920(OriginalListOwner* self) {
    clear_list_owner(self, [](void*) {});
}
extern "C" void THISCALL FUN_004d0f70(OriginalListOwner* self) {
    destroy_list_owner<FUN_004ce8a0>(self, dat_006c2be8);
}
extern "C" void THISCALL FUN_004d0fa0(OriginalListOwner* self) {
    destroy_list_owner<FUN_004ce920>(self, dat_006c2bec);
}
extern "C" void THISCALL FUN_004d0fd0(OriginalListOwner* self) {
    destroy_list_owner<FUN_004ce920>(self, dat_006c2bf0);
}
extern "C" void THISCALL FUN_004d1000(OriginalListOwner* self) {
    destroy_list_owner<FUN_004ce920>(self, dat_006c2bf4);
}

// Pointer-vector owner destructors, one per vtable.
extern "C" void THISCALL FUN_004d2170(OriginalPointerVectorOwner* self) {
    destroy_pointer_vector(self, dat_006c2bf8);
}
extern "C" void THISCALL FUN_004d21b0(OriginalPointerVectorOwner* self) {
    destroy_pointer_vector(self, dat_006c2bfc);
}
extern "C" void THISCALL FUN_004d21f0(OriginalPointerVectorOwner* self) {
    destroy_pointer_vector(self, dat_006c2c00);
}
extern "C" void THISCALL FUN_004d2230(OriginalPointerVectorOwner* self) {
    destroy_pointer_vector(self, dat_006c2c04);
}
extern "C" void THISCALL FUN_004d2270(OriginalPointerVectorOwner* self) {
    destroy_pointer_vector(self, dat_006c2c08);
}
extern "C" void THISCALL FUN_004d2860(OriginalPointerVectorOwner* self) {
    destroy_pointer_vector(self, dat_006c2c0c);
}
extern "C" void THISCALL FUN_004d28a0(OriginalPointerVectorOwner* self) {
    destroy_pointer_vector(self, dat_006c2c10);
}
extern "C" void THISCALL FUN_004d28e0(OriginalPointerVectorOwner* self) {
    destroy_pointer_vector(self, dat_006c2c14);
}
extern "C" void THISCALL FUN_004d2920(OriginalPointerVectorOwner* self) {
    destroy_pointer_vector(self, dat_006c2c18);
}
extern "C" void THISCALL FUN_004d2960(OriginalPointerVectorOwner* self) {
    destroy_pointer_vector(self, dat_006c2c1c);
}
extern "C" void THISCALL FUN_004d29a0(OriginalPointerVectorOwner* self) {
    destroy_pointer_vector(self, dat_006c2c20);
}
extern "C" void THISCALL FUN_0042c9e0(OriginalPointerVectorOwner* self) {
    destroy_pointer_vector(self, dat_006be5cc);
}
// 0x0042ca40: destructor of an object whose pointer-vector owner sits at +0x48.
extern "C" void THISCALL FUN_0042ca40(void* self) {
    destroy_pointer_vector(
        reinterpret_cast<OriginalPointerVectorOwner*>(static_cast<uint8_t*>(self) + 0x48),
        dat_006be5cc);
}
// 0x004d3ea0: the same shape, with elements destroyed by 0x0042ca80.
extern "C" void THISCALL FUN_004d3ea0(OriginalPointerVectorOwner* self) {
    self->vtable = dat_006c2ca4;
    FUN_0042ca80(self);
    release_pointer_storage(self);
}

// 0x005b3240: constructor of a pair of cleared words.
extern "C" void* THISCALL FUN_005b3240(void* self) {
    member_at(self, 0) = nullptr;
    member_at(self, 4) = nullptr;
    return self;
}
// 0x005b3250: read two square buffers sized by the global grid width: the
// first holds 2 * width * width bytes, the second width * width.
extern "C" bool THISCALL FUN_005b3250(void** self, const int32_t* descriptor) {
    uint32_t width = dat_0224e650;
    if (!FUN_0059fe40(descriptor, self[0], width * width * 2))
        return false;
    width = dat_0224e650;
    return FUN_0059fe40(descriptor, self[1], width * width);
}
