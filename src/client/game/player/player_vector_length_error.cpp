#include "core/filter_owner.h"

// 0x004d2aa0: throw std::length_error("vector<T> too long"). Byte-identical to
// 0x004375d0 apart from its EH handler, whose only unwind action destroys the
// same temporary message.
extern "C" [[noreturn]] void __attribute__((thiscall)) FUN_004d2aa0(void* self) {
    FUN_004375d0(self);
}
extern "C" [[noreturn]] void __attribute__((thiscall)) fun_004d2aa0(void* self)
    __attribute__((alias("FUN_004d2aa0")));
