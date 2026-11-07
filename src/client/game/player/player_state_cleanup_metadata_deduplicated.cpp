#include "runtime/cxx_exception.h"
extern "C" {
extern const unsigned char code_0042ca20[] __asm__("_FUN_0042ca20");
extern const unsigned char code_004a7720[] __asm__("_FUN_004a7720");
extern const unsigned char code_006ac520[] __asm__("_FUN_006ac520");
extern const void* const dat_006be5cc[1] = {code_0042ca20};
extern const void* const dat_006c1c58[1] = {code_004a7720};
// Unwind data of the original 0x0042c870 (see player_state_cleanup_auxiliary.cpp).
extern const CxxUnwindEntry dat_006fb1cc[1] = {{-1, const_cast<unsigned char*>(code_006ac520)}};
extern const CxxFuncInfo dat_006fb1d4 = {
    0x19930520, 1, const_cast<CxxUnwindEntry*>(dat_006fb1cc), 0, nullptr, 0, nullptr};
// The shared diagnostic literal belongs to canonical common_runtime_storage.
}
