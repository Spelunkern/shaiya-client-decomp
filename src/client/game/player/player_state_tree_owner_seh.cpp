#include "game/player/player_state_containers.h"

namespace {
// The original EH state covering the clear runs the map destructor on
// unwind (funclets 0x006b0fa0..0x006b1020).
struct TreeUnwind {
    OriginalTree12* tree;
    void THISCALL (*destroy)(OriginalTree12*);
    bool active = true;
    ~TreeUnwind() noexcept(false) {
        if (active)
            destroy(tree);
    }
};
// Map-owner destructor: set the vtable, clear the values under the unwind
// guard, then destroy the map itself.
void destroy_tree_owner(OriginalTreeOwner* self, const void* vtable,
                        void THISCALL (*clear)(OriginalTreeOwner*),
                        void THISCALL (*destroy)(OriginalTree12*)) {
    self->vtable = vtable;
    {
        TreeUnwind cleanup{&self->tree, destroy};
        clear(self);
        cleanup.active = false;
    }
    destroy(&self->tree);
}
} // namespace

extern "C" void THISCALL FUN_004d3f50(OriginalTreeOwner* self) {
    destroy_tree_owner(self, dat_006c2ca8, FUN_0042c6f0, FUN_004d3f00);
}
extern "C" void THISCALL FUN_004d59b0(OriginalTreeOwner* self) {
    destroy_tree_owner(self, dat_006c2cac, FUN_0048df40, FUN_004d4040);
}
extern "C" void THISCALL FUN_004d5a30(OriginalTreeOwner* self) {
    destroy_tree_owner(self, dat_006c2cb0, FUN_004d20f0, FUN_004d4070);
}
extern "C" void THISCALL FUN_004d5ab0(OriginalTreeOwner* self) {
    destroy_tree_owner(self, dat_006c2cb4, FUN_0042c770, FUN_004d40a0);
}
extern "C" void THISCALL FUN_004d5b30(OriginalTreeOwner* self) {
    destroy_tree_owner(self, dat_006c2cb8, FUN_0042c7f0, FUN_004d40d0);
}
