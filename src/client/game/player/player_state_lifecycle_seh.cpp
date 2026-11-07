#include "core/sentinel_list12.h"
#include "game/player/player_state_bytes.h"
#include "game/player/player_state_containers.h"
#include "runtime/allocations.h"

using namespace player_state_bytes;

extern "C" {
extern uint32_t DAT_007198f0;
extern uint32_t DAT_0071986c;
void THISCALL FUN_004d2170(void*);
void THISCALL FUN_004d21b0(void*);
void THISCALL FUN_004d21f0(void*);
void THISCALL FUN_004d2230(void*);
void THISCALL FUN_004d2270(void*);
void THISCALL FUN_004d2860(void*);
void THISCALL FUN_004d28a0(void*);
void THISCALL FUN_004d28e0(void*);
void THISCALL FUN_004d2920(void*);
void THISCALL FUN_004d2960(void*);
void THISCALL FUN_004d29a0(void*);
void THISCALL FUN_004d0f70(void*);
void THISCALL FUN_004d0fa0(void*);
void THISCALL FUN_004d0fd0(void*);
void THISCALL FUN_004d1000(void*);
}

namespace {
// The player state's members in construction order. Their index is the EH
// state shared by the constructor 0x004d5c30 and the destructor 0x004d66a0
// (unwind table 0x006ffd30): unwinding from state N destroys members N..0.
struct Member {
    uint32_t offset;
    void THISCALL (*destroy)(void*);
};
void THISCALL destroy_three_words(void* self) {
    FUN_00555f30(self);
}
const Member members[26] = {
    {0x8868, FUN_004ce860},
    {0x8898, FUN_004ce860},
    {0x88c8, FUN_004ce610},
    {0x1998180, FUN_004d2860},
    {0x1998198, FUN_004d28a0},
    {0x19981b0, FUN_004d28e0},
    {0x19981c8, reinterpret_cast<void THISCALL (*)(void*)>(FUN_004d3f50)},
    {0x19981dc, reinterpret_cast<void THISCALL (*)(void*)>(FUN_004d3ea0)},
    {0x19981f4, reinterpret_cast<void THISCALL (*)(void*)>(FUN_004d59b0)},
    {0x199822c, reinterpret_cast<void THISCALL (*)(void*)>(FUN_004d5a30)},
    {0x1998240, FUN_004d2170},
    {0x1998258, FUN_004d21b0},
    {0x1998270, FUN_004d21f0},
    {0x1998288, FUN_004d2230},
    {0x19982a0, FUN_004d2920},
    {0x19982b8, reinterpret_cast<void THISCALL (*)(void*)>(FUN_004d5ab0)},
    {0x19987f0, FUN_004d0f70},
    {0x1998804, FUN_004d0fa0},
    {0x199912c, FUN_004d0fd0},
    {0x1999760, FUN_004d1000},
    {0x199bd00, reinterpret_cast<void THISCALL (*)(void*)>(FUN_004d5b30)},
    {0x199bd14, FUN_004d2960},
    {0x199bd2c, FUN_004d29a0},
    {0x199c36c, FUN_004a7710},
    {0x199c3c0, FUN_004d2270},
    {0x199c624, destroy_three_words},
};
void destroy_member(void* self, int32_t state) {
    members[state].destroy(at(self, members[state].offset));
}
// Mirrors the original EH state variable: the code below updates it at the
// same points as the original, and an exception unwinds from it.
struct MemberUnwind {
    void* self;
    int32_t state;
    ~MemberUnwind() noexcept(false) {
        for (; state >= 0; --state)
            destroy_member(self, state);
    }
};

// Record of 40 bytes cleared field by field; bytes 32..34 and 39 are padding
// the original leaves untouched.
void clear_record40(void* self, uint32_t base) {
    set8(self, base, 0);
    set8(self, base + 1, 0);
    set8(self, base + 2, 0);
    set16(self, base + 3, 0);
    for (uint32_t offset = 5; offset != 11; ++offset)
        set8(self, base + offset, 0);
    for (uint32_t offset = 11; offset != 31; offset += 4)
        set32(self, base + offset, 0);
    set8(self, base + 31, 0);
    set32(self, base + 35, 0);
}
void clear_records40(void* self, uint32_t base, uint32_t count) {
    for (uint32_t index = 0; index != count; ++index)
        clear_record40(self, base + index * 0x28);
}
// The other 40-byte record layout (at +0x1998cc4); bytes 3 and 37..39 stay.
void clear_record40b(void* self, uint32_t base) {
    set8(self, base, 0);
    set8(self, base + 1, 0);
    set8(self, base + 2, 0);
    set16(self, base + 4, 0);
    set32(self, base + 0xc, 0);
    for (uint32_t offset = 6; offset != 0xc; ++offset)
        set8(self, base + offset, 0);
    for (uint32_t offset = 0x10; offset != 0x24; offset += 4)
        set32(self, base + offset, 0);
    set8(self, base + 0x24, 0);
}
// 16-byte action entry: active flag set, the rest cleared except byte 1.
void reset_actions16(void* self, uint32_t base, uint32_t count) {
    for (uint32_t index = 0; index != count; ++index) {
        const uint32_t entry = base + index * 0x10;
        set8(self, entry, 1);
        set16(self, entry + 2, 0);
        set32(self, entry + 4, 0);
        set32(self, entry + 8, 0);
        set32(self, entry + 0xc, 0);
    }
}
// The 0x004ce860 record: vtable and cleared fields.
void construct_record(void* self, uint32_t base) {
    set_pointer(self, base, dat_006c2bc0);
    set32(self, base + 4, 0);
    set32(self, base + 8, 0);
    set8(self, base + 0xc, 0);
    for (uint32_t offset = 0x10; offset != 0x30; offset += 8) {
        set32(self, base + offset, 0);
        set16(self, base + offset + 4, 0);
    }
}
// Pointer-vector owner: vtable, empty range and the trailing word.
void construct_vector_owner(void* self, uint32_t base, const void* vtable) {
    set_pointer(self, base, vtable);
    for (uint32_t offset = 8; offset != 0x18; offset += 4)
        set32(self, base + offset, 0);
}
// Map owner: vtable, then a fresh header that is its own parent and children.
void construct_tree_owner(void* self, uint32_t base, const void* vtable) {
    set_pointer(self, base, vtable);
    OriginalTreeNode24* header = FUN_0053a7e0();
    set_pointer(self, base + 8, header);
    header->is_header = 1;
    header->parent = header;
    header->left = header;
    header->right = header;
    set32(self, base + 0xc, 0);
    set32(self, base + 0x10, 0);
}
// List owner: vtable, then a fresh sentinel node.
void construct_list_owner(void* self, uint32_t base, const void* vtable) {
    set_pointer(self, base, vtable);
    set_pointer(self, base + 8, FUN_004ce840());
    set32(self, base + 0xc, 0);
    set32(self, base + 0x10, 0);
}
} // namespace

// 0x004d5c30: construct the player state in place.
extern "C" void* THISCALL FUN_004d5c30(void* self) {
    MemberUnwind unwind{self, -1};
    for (uint32_t offset = 0xac; offset != 0xdc; offset += 4)
        set32(self, offset, 0);
    clear_records40(self, 0x10f5, 0x90);
    clear_records40(self, 0x37fd, 0x100);
    clear_records40(self, 0x6001, 0xf0);
    clear_records40(self, 0x8585, 0x10);
    for (uint32_t offset = 0x8804; offset != 0x881c; ++offset)
        set8(self, offset, 0);
    construct_record(self, 0x8868);
    unwind.state = 0;
    construct_record(self, 0x8898);
    unwind.state = 1;
    FUN_004ce570(at(self, 0x88c8));
    FUN_004cc0d0(at(self, 0x1988974));
    construct_vector_owner(self, 0x1998180, dat_006c2c0c);
    construct_vector_owner(self, 0x1998198, dat_006c2c10);
    construct_vector_owner(self, 0x19981b0, dat_006c2c14);
    unwind.state = 5;
    construct_tree_owner(self, 0x19981c8, dat_006c2ca8);
    construct_vector_owner(self, 0x19981dc, dat_006c2ca4);
    unwind.state = 7;
    construct_tree_owner(self, 0x19981f4, dat_006c2cac);
    set32(self, 0x1998208, 0xffffffff);
    set32(self, 0x199820c, 0xffffffff);
    set32(self, 0x1998210, 0);
    set32(self, 0x1998214, 0);
    set32(self, 0x1998218, 0);
    set8(self, 0x199821c, 0);
    set32(self, 0x1998220, 0xffffffff);
    set32(self, 0x1998224, 0xffffffff);
    set32(self, 0x1998228, 0xffffffff);
    unwind.state = 8;
    construct_tree_owner(self, 0x199822c, dat_006c2cb0);
    construct_vector_owner(self, 0x1998240, dat_006c2bf8);
    construct_vector_owner(self, 0x1998258, dat_006c2bfc);
    construct_vector_owner(self, 0x1998270, dat_006c2c00);
    construct_vector_owner(self, 0x1998288, dat_006c2c04);
    construct_vector_owner(self, 0x19982a0, dat_006c2c18);
    unwind.state = 14;
    construct_tree_owner(self, 0x19982b8, dat_006c2cb4);
    unwind.state = 15;
    reset_actions16(self, 0x19982f0, 0x32);
    reset_actions16(self, 0x1998614, 0x18);
    reset_actions16(self, 0x19987a8, 2);
    construct_list_owner(self, 0x19987f0, dat_006c2be8);
    unwind.state = 16;
    construct_list_owner(self, 0x1998804, dat_006c2bec);
    unwind.state = 17;
    clear_records40(self, 0x1998939, 0xd);
    for (uint32_t index = 0; index != 0x14; ++index)
        set32(self, 0x1998bd4 + index * 0xc, 0);
    for (uint32_t index = 0; index != 0x14; ++index)
        clear_record40b(self, 0x1998cc4 + index * 0x28);
    construct_list_owner(self, 0x199912c, dat_006c2bf0);
    zero32(self, 0x19996f8, 0x10);
    unwind.state = 18;
    construct_list_owner(self, 0x1999760, dat_006c2bf4);
    unwind.state = 19;
    clear_records40(self, 0x199977d, 0xf0);
    construct_tree_owner(self, 0x199bd00, dat_006c2cb8);
    construct_vector_owner(self, 0x199bd14, dat_006c2c1c);
    construct_vector_owner(self, 0x199bd2c, dat_006c2c20);
    unwind.state = 22;
    clear_records40(self, 0x199bdcd, 0x24);
    FUN_004a76f0(at(self, 0x199c36c));
    construct_vector_owner(self, 0x199c3c0, dat_006c2c08);
    set32(self, 0x199c628, 0);
    set32(self, 0x199c62c, 0);
    set32(self, 0x199c630, 0);
    unwind.state = 25;

    set32(self, 0x199bd44, 0);
    set8(self, 0x199bdbc, 0);
    set32(self, 0x199bdb0, 0);
    set32(self, 0x199bdb8, 0);
    set32(self, 0x199bdb4, 0);
    set32(self, 0x199bdc0, 0);
    set32(self, 0x199bdc4, 0);
    set8(self, 0x199bdc8, 0x1e);
    set32(self, 0x199bda8, 0);
    set32(self, 0x199bdac, 0);
    zero32(self, 0x199bd48, 0x18);
    set32(self, 0x1999080, 0);
    set16(self, 0x1999008, 0);
    set32(self, 4, 0);
    set8(self, 0x19982ee, 0);
    set8(self, 0, 0x64);
    // Experience thresholds.
    zero32(self, 0x1998818, 0x14);
    set32(self, 0x1998818, 15000);
    set32(self, 0x1998820, 25000);
    set32(self, 0x1998824, 30000);
    set32(self, 0x1998828, 60000);
    set32(self, 0x199882c, 120000);
    set32(self, 0x1998830, 180000);
    set32(self, 0x1998834, 240000);
    set32(self, 0x1998838, 300000);
    set32(self, 0x199883c, 600000);
    set32(self, 0x1998840, 2000);
    set32(self, 0x199881c, 20000);
    set32(self, 0x1998908, 20000);
    zero32(self, 0x199901c, 6);
    set8(self, 0x1999034, 0);
    set32(self, 0x37f8, 0);
    for (uint32_t offset = 0x1999108; offset != 0x1999120; offset += 4)
        set32(self, offset, 0);
    set32(self, 0x1999120, 0xffffffff);
    zero32(self, 0x1998b54, 0x20);
    set8(self, 0x1999124, 0);
    set8(self, 0x1999125, 0);
    set32(self, 0x1999128, 0);
    set8(self, 0x1999140, 0);
    set32(self, 0x1999144, 0);
    set32(self, 0x1999148, 0);
    set32(self, 0x1999778, DAT_007198f0);
    set32(self, 0x1999774, DAT_0071986c);
    for (uint32_t offset = 0x199c388; offset != 0x199c38d; ++offset)
        set8(self, offset, 0);
    set32(self, 0x199c3a0, 0);
    set32(self, 0x199c3a4, 0);
    FUN_004d4100(self);
    set8(self, 0x199c3a8, 6);
    for (uint32_t offset = 0x199c3a9; offset != 0x199c3bd; offset += 4)
        set32(self, offset, 0);
    set8(self, 0x199c3bd, 0);
    set32(self, 0x199c61c, 0xffffffff);
    set8(self, 0x199c620, 0xff);
    unwind.state = -1;
    return self;
}

// 0x004d66a0: destroy the player state. The body clears the containers,
// then the members are destroyed in reverse order; the EH state is updated
// only where the original updates it.
extern "C" void THISCALL FUN_004d66a0(void* self) {
    MemberUnwind unwind{self, 25};
    auto vector = [self](uint32_t offset) {
        return static_cast<OriginalPointerVectorOwner*>(at(self, offset));
    };
    auto tree = [self](uint32_t offset) { return static_cast<OriginalTreeOwner*>(at(self, offset)); };
    FUN_0042c680(vector(0x199bd2c));
    FUN_0042c7f0(tree(0x199bd00));
    FUN_0042c680(vector(0x199bd14));
    FUN_0042c680(vector(0x1998258));
    if (get32(self, 0x199bdc4) != 0) {
        fun_005cdc59(reinterpret_cast<void*>(get32(self, 0x199bdc4)));
        set32(self, 0x199bdc4, 0);
    }
    FUN_004ce8a0(static_cast<OriginalListOwner*>(at(self, 0x19987f0)));
    FUN_0042ca80(vector(0x19981dc));
    FUN_0048df40(tree(0x19981f4));
    FUN_0042c680(vector(0x19981b0));
    FUN_0042c680(vector(0x1998198));
    FUN_0042c680(vector(0x1998180));
    FUN_0042c6f0(tree(0x19981c8));
    FUN_004d20f0(tree(0x199822c));
    FUN_0042c680(vector(0x1998240));
    FUN_0042c680(vector(0x1998258));
    FUN_0042c680(vector(0x199c3c0));
    // The three-word vector at +0x199c624 is freed and cleared twice: by the
    // body and by its inlined destructor, which then finds nothing to free.
    if (get32(self, 0x199c628) != 0)
        fun_005cc620(reinterpret_cast<void*>(get32(self, 0x199c628)));
    set32(self, 0x199c628, 0);
    set32(self, 0x199c62c, 0);
    set32(self, 0x199c630, 0);

    destroy_member(self, 24);
    unwind.state = 22;
    destroy_member(self, 23);
    destroy_member(self, 22);
    destroy_member(self, 21);
    unwind.state = 19;
    destroy_member(self, 20);
    for (int32_t state = 19; state != 15; --state)
        destroy_member(self, state);
    unwind.state = 14;
    destroy_member(self, 15);
    for (int32_t state = 14; state != 9; --state)
        destroy_member(self, state);
    unwind.state = 8;
    destroy_member(self, 9);
    unwind.state = 7;
    destroy_member(self, 8);
    destroy_member(self, 7);
    unwind.state = 5;
    destroy_member(self, 6);
    for (int32_t state = 5; state != 2; --state)
        destroy_member(self, state);
    unwind.state = 1;
    destroy_member(self, 2);
    unwind.state = 0;
    destroy_member(self, 1);
    unwind.state = -1;
    destroy_member(self, 0);
}

// 0x004ce610: destructor of the 0x004ce570 object. The four 8-byte records
// at +0x198008c are destroyed on unwind too (funclet 0x006b0f30).
namespace {
struct RecordsUnwind {
    void* self;
    bool active = true;
    ~RecordsUnwind() noexcept(false) {
        if (active)
            fun_005cdcbc(at(self, 0x198008c), 8, 4,
                         reinterpret_cast<OriginalElementCallback>(FUN_004ce530));
    }
};
void release_array(void* self, uint32_t offset, uint32_t element_size,
                   void THISCALL (*destructor)(void*)) {
    if (get32(self, offset) != 0) {
        original_delete_array(reinterpret_cast<void*>(get32(self, offset)), element_size,
                              reinterpret_cast<OriginalElementCallback>(destructor));
        set32(self, offset, 0);
    }
}
} // namespace
extern "C" void THISCALL FUN_004ce610(void* self) {
    set_pointer(self, 0, dat_006c2be4);
    {
        RecordsUnwind unwind{self};
        release_array(self, 8, 0x284, FUN_0047a880);
        release_array(self, 0x10, 0x1fc, FUN_0047a900);
        release_array(self, 0x88, 0xe4, FUN_0047a990);
        for (uint32_t index = 0; index != 14; ++index)
            release_array(self, 0x4c + index * 4, 0x1b4, FUN_0047a810);
        unwind.active = false;
    }
    fun_005cdcbc(at(self, 0x198008c), 8, 4, reinterpret_cast<OriginalElementCallback>(FUN_004ce530));
}
