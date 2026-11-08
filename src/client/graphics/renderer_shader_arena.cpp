#include <stdint.h>
// The shader compiler's bump arena. Only the original field offsets are
// recovered: +0x00 the newest reserved region (each region's first dword
// links to the previous one), +0x04 the commit granularity, +0x08 the used
// offset in the newest region, +0x0c its committed bytes, +0x10 its reserved
// bytes and +0x14 the default region size.
namespace {
using u32 = uint32_t;
u32 word(u32 p) {
    u32 result;
    __asm__ volatile("movl (%1),%0" : "=r"(result) : "r"(p) : "memory");
    return result;
}
void put(u32 p, u32 v) {
    __asm__ volatile("movl %1,(%0)" ::"r"(p), "r"(v) : "memory");
}
u32 address(const void* p) {
    return static_cast<u32>(reinterpret_cast<uintptr_t>(p));
}
void* pointer(u32 p) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(p));
}
// Rounds `value` up to a multiple of the power of two `unit`, modulo 2^32.
u32 round_up(u32 value, u32 unit) {
    return (unit + value - 1) & ~(unit - 1);
}
constexpr u32 mem_commit = 0x1000, mem_reserve = 0x2000, mem_release = 0x8000;
constexpr u32 page_readwrite = 4;
} // namespace
extern "C" {
__declspec(dllimport) void __attribute__((stdcall)) GetSystemInfo(void*);
__declspec(dllimport) void* __attribute__((stdcall)) VirtualAlloc(void*, uint32_t, uint32_t,
                                                                  uint32_t);
__declspec(dllimport) int32_t __attribute__((stdcall)) VirtualFree(void*, uint32_t, uint32_t);
// The arena every shader node is allocated from (renderer_shader_storage.cpp).
extern uint32_t dat_02251784;
void* __attribute__((thiscall)) FUN_00651bb5(void*, u32, u32);
void __attribute__((thiscall)) FUN_00651c0d(void*);
void* __attribute__((thiscall)) FUN_00651c2f(void*, u32, u32);
void* __attribute__((stdcall)) FUN_00604310(u32) __asm__("_FUN_00604310");
}
// Original 00651bb5..00651c0d (thiscall, RET 8): constructor. The commit
// granularity is `granularity` rounded up to the system page size, and the
// default region `region` (1 MiB when 0) rounded up to that granularity. The
// original reads stale frame bytes if GetSystemInfo writes nothing; it
// always writes the page size.
extern "C" void* __attribute__((thiscall)) FUN_00651bb5(void* object, u32 region, u32 granularity) {
    const u32 self = address(object);
    u32 info[9];
    put(self, 0);
    GetSystemInfo(info);
    const u32 page = info[1];
    put(self + 8, 0);
    put(self + 0x0c, 0);
    put(self + 0x10, 0);
    put(self + 4, page);
    const u32 unit = round_up(granularity, page);
    put(self + 4, unit);
    put(self + 0x14, round_up(region ? region : 0x100000, unit));
    return object;
}
// Original 00651c0d..00651c2f (thiscall, RET): releases every region, newest
// first, unlinking each before its VirtualFree.
extern "C" void __attribute__((thiscall)) FUN_00651c0d(void* object) {
    const u32 self = address(object);
    while (const u32 region = word(self)) {
        put(self, word(region));
        VirtualFree(pointer(region), 0, mem_release);
    }
}
// Original 00651c2f..00651d19 (thiscall, RET 8): `bytes` aligned to the
// power of two `align`. A block that fits the newest region only commits
// more pages; otherwise a new region is reserved, at least the default size
// and doubled until the block fits (modulo 2^32, so the original spins
// forever once the doubling wraps), with its first dword linking the previous
// region. NULL when VirtualAlloc fails; a new region that cannot be
// committed is released first.
extern "C" void* __attribute__((thiscall)) FUN_00651c2f(void* object, u32 bytes, u32 align) {
    const u32 self = address(object);
    const u32 mask = ~(align - 1);
    u32 end = ((align + word(self + 8) - 1) & mask) + bytes;
    if (end <= word(self + 0x10)) {
        const u32 committed = word(self + 0x0c);
        if (end > committed) {
            const u32 target = round_up(end, word(self + 4));
            if (!VirtualAlloc(pointer(word(self) + committed), target - committed, mem_commit,
                              page_readwrite))
                return nullptr;
            put(self + 0x0c, target);
        }
    } else {
        end = ((align + 3) & mask) + bytes;
        u32 reserved = word(self + 0x14);
        while (end > reserved) {
            reserved *= 2;
            __asm__ volatile("" ::: "memory");  // keeps the original endless loop observable
        }
        const u32 region = address(VirtualAlloc(nullptr, reserved, mem_reserve, page_readwrite));
        if (!region)
            return nullptr;
        const u32 target = round_up(end, word(self + 4));
        if (!VirtualAlloc(pointer(region), target, mem_commit, page_readwrite)) {
            VirtualFree(pointer(region), 0, mem_release);
            return nullptr;
        }
        put(region, word(self));
        put(self, region);
        put(self + 0x10, reserved);
        put(self + 0x0c, target);
    }
    const u32 block = word(self) - bytes;
    put(self + 8, end);
    return pointer(block + end);
}
// Original 00604310..00604329 (RET 4): 16-byte aligned block from the shared
// shader arena.
extern "C" void* __attribute__((stdcall)) FUN_00604310(u32 bytes) {
    return FUN_00651c2f(pointer(dat_02251784), bytes, 0x10);
}
