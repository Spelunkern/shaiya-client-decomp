#include "graphics/surface/surface_empty_callback.h"
#include "graphics/surface/surface_filter_storage.h"
#include "runtime/allocations.h"
// Source-side helpers of the shader compiler: the global compiler lock,
// memory-mapped source files, list nodes and the per-source object that
// owns an opened ID3DXInclude block. Only the original offsets are recovered.
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
// REP STOSD of `dwords` zero dwords.
void zero_dwords(u32 to, u32 dwords) {
    __asm__ volatile("rep stosl" : "+D"(to), "+c"(dwords) : "a"(0) : "memory");
}
constexpr u32 invalid_handle = 0xffffffffu;
} // namespace
extern "C" {
__declspec(dllimport) int32_t __attribute__((stdcall)) InterlockedCompareExchange(volatile void*,
                                                                                 int32_t, int32_t);
__declspec(dllimport) int32_t __attribute__((stdcall)) InterlockedExchange(volatile void*, int32_t);
__declspec(dllimport) void __attribute__((stdcall)) Sleep(uint32_t);
__declspec(dllimport) void __attribute__((stdcall)) InitializeCriticalSection(void*);
__declspec(dllimport) void __attribute__((stdcall)) EnterCriticalSection(void*);
__declspec(dllimport) void __attribute__((stdcall)) LeaveCriticalSection(void*);
__declspec(dllimport) void __attribute__((stdcall)) DeleteCriticalSection(void*);
__declspec(dllimport) int32_t __attribute__((stdcall)) GetVersionExA(void*);
__declspec(dllimport) int32_t __attribute__((stdcall))
WideCharToMultiByte(uint32_t, uint32_t, const void*, int32_t, void*, int32_t, const void*, void*);
__declspec(dllimport) void* __attribute__((stdcall))
CreateFileA(const void*, uint32_t, uint32_t, void*, uint32_t, uint32_t, void*);
__declspec(dllimport) void* __attribute__((stdcall))
CreateFileW(const void*, uint32_t, uint32_t, void*, uint32_t, uint32_t, void*);
__declspec(dllimport) void* __attribute__((stdcall))
CreateFileMappingA(void*, void*, uint32_t, uint32_t, uint32_t, const void*);
__declspec(dllimport) uint32_t __attribute__((stdcall)) GetFileSize(void*, uint32_t*);
__declspec(dllimport) void* __attribute__((stdcall))
MapViewOfFile(void*, uint32_t, uint32_t, uint32_t, uint32_t);
__declspec(dllimport) int32_t __attribute__((stdcall)) UnmapViewOfFile(const void*);
__declspec(dllimport) int32_t __attribute__((stdcall)) CloseHandle(void*);
__declspec(dllimport) uint32_t __attribute__((stdcall)) GetLastError();
__declspec(dllimport) int32_t __attribute__((stdcall)) DeleteObject(void*);
// The compiler lock: critical section, its user count and the spin word
// that guards both (renderer_shader_storage.cpp).
extern const unsigned char lock_section[] __asm__("_dat_0224ffa0");
extern uint32_t dat_0224ffb8, dat_0224ffbc;
void FUN_00602c9c();
void FUN_00602cf1();
void* __attribute__((thiscall)) FUN_00602d42(void*, u32, u32, u32);
void* __attribute__((thiscall)) FUN_00602d66(void*, u32, u32);
void __attribute__((thiscall)) FUN_00602d84(void*);
void* __attribute__((thiscall)) FUN_00602d9e(void*, u32);
void* __attribute__((thiscall)) FUN_00602dbf(void*);
void __attribute__((thiscall)) FUN_00602df4(void*);
void* __attribute__((thiscall)) FUN_00602e4d(void*, u32);
void* __attribute__((thiscall)) FUN_005fc6f3(void*);
u32 __attribute__((thiscall)) FUN_005fc705(void*, const void*, u32);
u32 __attribute__((thiscall)) FUN_005fc843(void*);
void __attribute__((thiscall)) FUN_005fc886(void*);
void __attribute__((thiscall)) FUN_005fc89e(void*);
u32 __attribute__((thiscall)) FUN_0060690b(void*);
void* __attribute__((thiscall)) FUN_0060690f(void*);
}
namespace {
// Spins (Sleep(1) between tries) until the guard word goes from 0 to 1.
void acquire_guard() {
    while (InterlockedCompareExchange(&dat_0224ffbc, 1, 0) == 1)
        Sleep(1);
}
void* section() {
    return const_cast<unsigned char*>(lock_section);
}
} // namespace
// Original 00602c9c..00602cf1 (cdecl): enter the compiler lock, creating
// the critical section for its first user.
extern "C" void FUN_00602c9c() {
    acquire_guard();
    if (!dat_0224ffb8)
        InitializeCriticalSection(section());
    dat_0224ffb8 += 1;
    InterlockedExchange(&dat_0224ffbc, 0);
    EnterCriticalSection(section());
}
// Original 00602cf1..00602d42 (cdecl): leave the compiler lock, deleting the
// critical section after its last user.
extern "C" void FUN_00602cf1() {
    LeaveCriticalSection(section());
    acquire_guard();
    if (!(dat_0224ffb8 -= 1))
        DeleteCriticalSection(section());
    InterlockedExchange(&dat_0224ffbc, 0);
}
// Original 00602d42..00602d66 (thiscall, RET 12): three-field list node.
extern "C" void* __attribute__((thiscall)) FUN_00602d42(void* object, u32 a, u32 b, u32 c) {
    const u32 self = address(object);
    put(self + 0x0c, 0);
    put(self + 0x10, 0);
    put(self, a);
    put(self + 4, b);
    put(self + 8, c);
    return object;
}
// Original 00602d66..00602d84 (thiscall, RET 8): two-field list node.
extern "C" void* __attribute__((thiscall)) FUN_00602d66(void* object, u32 a, u32 b) {
    const u32 self = address(object);
    put(self + 8, 0);
    put(self + 0x0c, 0);
    put(self, a);
    put(self + 4, b);
    return object;
}
// Original 00602d84..00602d9e (thiscall, RET): frees the rest of the chain
// linked through +0x0c, deepest node first.
extern "C" void __attribute__((thiscall)) FUN_00602d84(void* object) {
    if (const u32 next = word(address(object) + 0x0c)) {
        FUN_00602d84(pointer(next));
        fun_005cc620(pointer(next));
    }
}
// Original 00602d9e..00602dbf (thiscall, RET 4): scalar deleting destructor.
extern "C" void* __attribute__((thiscall)) FUN_00602d9e(void* object, u32 flags) {
    FUN_00602d84(object);
    if (flags & 1)
        fun_005cc620(object);
    return object;
}
// Original 00602dbf..00602df4 (thiscall, RET): source object constructor:
// the header fields, the mapped file at +0x3c, the member at +0x4c and the
// include and child fields cleared.
extern "C" void* __attribute__((thiscall)) FUN_00602dbf(void* object) {
    const u32 self = address(object);
    FUN_0060690f(object);
    FUN_005fc6f3(pointer(self + 0x3c));
    FUN_005fc891(pointer(self + 0x4c));
    static const unsigned char zeroed[7] = {0x38, 0x58, 0x64, 0x68, 0x5c, 0x60, 0x6c};
    for (u32 i = 0; i < 7; ++i)
        put(self + zeroed[i], 0);
    return object;
}
// Original 00602df4..00602e4d (thiscall, RET): source object destructor:
// the node list at +0x38, the child source at +0x6c, the include block
// (ID3DXInclude::Close of +0x64 on +0x58), the member at +0x4c and the
// mapped file, then the empty base destructor the original tail-jumps to.
extern "C" void __attribute__((thiscall)) FUN_00602df4(void* object) {
    const u32 self = address(object);
    if (const u32 list = word(self + 0x38))
        FUN_00602d9e(pointer(list), 1);
    if (const u32 child = word(self + 0x6c)) {
        FUN_00602df4(pointer(child));
        fun_005cc620(pointer(child));
    }
    if (word(self + 0x58)) {
        if (const u32 data = word(self + 0x64)) {
            const u32 include = word(self + 0x58);
            using Close = u32(__attribute__((stdcall))*)(u32, u32);
            reinterpret_cast<Close>(word(word(include) + 4))(include, data);
        }
    }
    FUN_005fc89e(pointer(self + 0x4c));
    FUN_005fc886(pointer(self + 0x3c));
    FUN_0069dae1();
}
// Original 00602e4d..00602e6c (thiscall, RET 4): scalar deleting destructor.
extern "C" void* __attribute__((thiscall)) FUN_00602e4d(void* object, u32 flags) {
    FUN_00602df4(object);
    if (flags & 1)
        fun_005cc620(object);
    return object;
}
// Original 005fc6f3..005fc705 (thiscall, RET): closed mapped file
// {file, mapping, view, size}.
extern "C" void* __attribute__((thiscall)) FUN_005fc6f3(void* object) {
    const u32 self = address(object);
    put(self, invalid_handle);
    put(self + 4, invalid_handle);
    put(self + 8, 0);
    put(self + 0x0c, 0);
    return object;
}
// Original 005fc705..005fc843 (thiscall, RET 8): maps `path` read-only.
// A wide path on a non-NT system is converted to UTF-8 and opened with
// CreateFileA. As in the original, the mapping handle is compared with
// INVALID_HANDLE_VALUE, not NULL. An empty file has no view. Failures return
// 0x88760b59 after GetLastError (its result is ignored). The original reads
// stale frame bytes if GetVersionExA fails; here the platform is zero.
extern "C" u32 __attribute__((thiscall)) FUN_005fc705(void* object, const void* path, u32 wide) {
    const u32 self = address(object);
    if (!path)
        return 0x8876086c;
    u32 converted = 0;
    if (wide) {
        u32 info[37];
        zero_dwords(address(info), 37);
        info[0] = 0x94;
        GetVersionExA(info);
        if (info[4] != 2) {
            const int32_t bytes = WideCharToMultiByte(0xfde9, 0, path, -1, nullptr, 0, nullptr, nullptr);
            converted = address(fun_005cc96d(static_cast<u32>(bytes)));
            if (!converted)
                return 0x8007000e;
            WideCharToMultiByte(0xfde9, 0, path, -1, pointer(converted), bytes, nullptr, nullptr);
            path = pointer(converted);
            wide = 0;
        }
    }
    void* const file = wide ? CreateFileW(path, 0x80000000u, 1, nullptr, 3, 0x10000000, nullptr)
                            : CreateFileA(path, 0x80000000u, 1, nullptr, 3, 0x10000000, nullptr);
    put(self, address(file));
    fun_005cc620(pointer(converted));
    if (word(self) != invalid_handle) {
        const u32 mapping =
            address(CreateFileMappingA(pointer(word(self)), nullptr, 2, 0, 0, nullptr));
        put(self + 4, mapping);
        if (mapping != invalid_handle) {
            const u32 size = GetFileSize(pointer(word(self)), nullptr);
            put(self + 0x0c, size);
            if (size != invalid_handle) {
                if (!size) {
                    put(self + 8, 0);
                    return 0;
                }
                const u32 view = address(MapViewOfFile(pointer(word(self + 4)), 4, 0, 0, 0));
                put(self + 8, view);
                if (view)
                    return 0;
            }
        }
    }
    GetLastError();
    return 0x88760b59;
}
// Original 005fc843..005fc886 (thiscall, RET): unmaps the view and closes
// the mapping and the file, marking each closed.
extern "C" u32 __attribute__((thiscall)) FUN_005fc843(void* object) {
    const u32 self = address(object);
    if (const u32 view = word(self + 8)) {
        UnmapViewOfFile(pointer(view));
        put(self + 8, 0);
        put(self + 0x0c, 0);
    }
    if (const u32 mapping = word(self + 4); mapping != invalid_handle) {
        CloseHandle(pointer(mapping));
        put(self + 4, invalid_handle);
    }
    if (const u32 file = word(self); file != invalid_handle) {
        CloseHandle(pointer(file));
        put(self, invalid_handle);
    }
    return 0;
}
// Original 005fc886..005fc891 (thiscall, RET): closes an open mapped file.
// Callers ignore EAX.
extern "C" void __attribute__((thiscall)) FUN_005fc886(void* object) {
    if (word(address(object)) != invalid_handle)
        FUN_005fc843(object);
}
// Original 005fc89e..005fc8ac (thiscall, RET): DeleteObject on a non-NULL
// handle. Callers ignore EAX.
extern "C" void __attribute__((thiscall)) FUN_005fc89e(void* object) {
    if (const u32 handle = word(address(object)))
        DeleteObject(pointer(handle));
}
// Original 0060690b..0060690f (thiscall, RET).
extern "C" u32 __attribute__((thiscall)) FUN_0060690b(void* object) {
    return word(address(object) + 8);
}
// Original 0060690f..00606929 (thiscall, RET): header fields of a source
// object; +0x1c starts at 1.
extern "C" void* __attribute__((thiscall)) FUN_0060690f(void* object) {
    const u32 self = address(object);
    put(self, 0);
    put(self + 4, 0);
    put(self + 0x18, 0);
    put(self + 0x1c, 1);
    put(self + 0x2c, 0);
    put(self + 0x30, 0);
    return object;
}
