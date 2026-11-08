#include "graphics/render_debug_mute.h"
#include "graphics/surface/surface_convert.h"
#include "graphics/surface/surface_final_blocks.h"

// Volume loading: lock a Direct3D 9 volume into a transfer descriptor and run
// the shared surface transfer on it.
namespace {
using u32 = uint32_t;
using i32 = int32_t;
constexpr i32 invalid_call = i32(0x8876086c);

u32 word(const void* p) {
    u32 v;
    __asm__ volatile("movl (%1),%0" : "=r"(v) : "r"(p) : "memory");
    return v;
}
void put(void* p, u32 v) {
    __asm__ volatile("movl %1,(%0)" ::"r"(p), "r"(v) : "memory");
}
void* ptr(u32 v) {
    return reinterpret_cast<void*>(v);
}
u32 at(const void* base, u32 offset) {
    return word(static_cast<const unsigned char*>(base) + offset);
}
void set(void* base, u32 offset, u32 value) {
    put(static_cast<unsigned char*>(base) + offset, value);
}
void copy_words(void* to, const void* from, u32 count) {
    for (u32 i = 0; i != count; ++i)
        set(to, i * 4, at(from, i * 4));
}
// COM call through slot `offset` of the object's vtable (stdcall, object first).
template <class... Args> i32 com(u32 object, u32 offset, Args... args) {
    using Method = i32(__attribute__((stdcall))*)(void*, Args...);
    return reinterpret_cast<Method>(word(ptr(word(ptr(object)) + offset)))(ptr(object), args...);
}

// D3DVOLUME_DESC and D3DBOX as the original reads them.
struct VolumeDesc {
    u32 format, type, usage, pool, width, height, depth;
};
struct Box {
    u32 left, top, right, bottom, front, back;
};
} // namespace
extern "C" const uint32_t dat_006f0014[4];

// 0x005f04e1: empty volume holder.
extern "C" void* __attribute__((thiscall)) FUN_005f04e1(void* self) {
    set(self, 0, 0);
    return self;
}
// 0x005f04e7: unlock (slot 0x28) and release the held volume.
extern "C" u32 __attribute__((thiscall)) FUN_005f04e7(void* self) {
    if (at(self, 0) != 0) {
        com(at(self, 0), 0x28);
        if (at(self, 0) != 0) {
            com(at(self, 0), 8);
            set(self, 0, 0);
        }
    }
    return 0;
}
// 0x005f09ff: the holder's destructor, a jump to 0x005f04e7.
extern "C" u32 __attribute__((thiscall)) FUN_005f09ff(void* self) {
    return FUN_005f04e7(self);
}

// 0x005f0a04: lock `volume` (optionally only `box`) and describe it in the
// 0x54-byte transfer descriptor `out`. Bit 0 of `flags` asks for read-only
// access, bit 16 marks the volume as the transfer source too, and bit 17
// allows discarding a dynamic volume. Compressed and packed YUV formats lock
// whole 4x4 blocks or pixel pairs, and the box becomes relative to the lock.
extern "C" i32 __attribute__((thiscall))
FUN_005f0a04(void* self, void* out, u32 volume, u32 cookie, const Box* box_argument, u32 extra,
              u32 flags) {
    if (at(self, 0) != 0)
        FUN_005f04e7(self);
    VolumeDesc desc;
    com(volume, 0x20, &desc);
    Box box;
    u32 partial, writable;
    if (box_argument != nullptr) {
        copy_words(&box, box_argument, 6);
        if (box.right > desc.width || box.left > box.right || box.bottom > desc.height ||
            box.top > box.bottom || box.back > desc.depth || box.front > box.back)
            return invalid_call;
        partial = box.left == 0 && box.right == desc.width && box.top == 0 &&
                          box.bottom == desc.height && box.front == 0 && box.back == desc.depth
                      ? 0
                      : 1;
        writable = !(flags & 1) && partial == 0 ? 1 : 0;
    } else {
        box = {0, 0, desc.width, desc.height, 0, desc.depth};
        partial = 0;
        writable = ~flags & 1;
    }
    if (desc.pool == 0 && !(desc.usage & 0x200))
        return invalid_call;

    u32 lock_flags = ((flags & 1) | 0x80) << 4;
    u32 saved_flags = lock_flags;
    Box aligned;
    bool lock_box = false;
    if (writable && (desc.usage & 0x200) && (flags & 0x20000)) {
        lock_flags |= 0x2000;
    } else {
        if (writable && (desc.usage & 0x200)) {
            FUN_005e9b1d(1);
            u32 unknown = 0;
            if (com(volume, 0x1c, dat_006f0014, &unknown) >= 0) {
                if (com(unknown, 0x34) == 1) {
                    lock_flags |= 0x2000;
                    saved_flags = lock_flags;
                    partial = 0;
                }
                if (unknown != 0)
                    com(unknown, 8);
            }
            FUN_005e9b1d(0);
        }
        if (partial != 0) {
            switch (desc.format) {
            case 0x31545844: // DXT1..DXT5: whole 4x4 blocks
            case 0x32545844:
            case 0x33545844:
            case 0x34545844:
            case 0x35545844: {
                aligned = {box.left & ~3u, box.top & ~3u, (box.right + 3) & ~3u,
                           (box.bottom + 3) & ~3u, box.front, box.back};
                if (aligned.right > desc.width)
                    aligned.right = desc.width;
                if (aligned.bottom > desc.height)
                    aligned.bottom = desc.height;
                lock_box = !(aligned.left == 0 && aligned.right == desc.width && aligned.top == 0 &&
                             aligned.bottom == desc.height && aligned.front == 0 &&
                             box.back == desc.depth);
                lock_flags = saved_flags;
                break;
            }
            case 0x32595559: // YUY2, UYVY, RGBG, GRGB: pixel pairs
            case 0x59565955:
            case 0x42475247:
            case 0x47424752:
                aligned = {box.left & ~1u, box.top, (box.right + 1) & ~1u, box.bottom, box.front,
                           box.back};
                if (aligned.right > desc.width)
                    aligned.right = desc.width;
                lock_box = !(aligned.left == 0 && aligned.right == desc.width && box.top == 0 &&
                             box.bottom == desc.height && box.front == 0 && box.back == desc.depth);
                break;
            default:
                copy_words(&aligned, &box, 6);
                lock_box = true;
                lock_flags = saved_flags;
                break;
            }
        }
    }

    u32 locked[3]; // RowPitch, SlicePitch, pBits
    const i32 result = lock_box ? com(volume, 0x24, locked, &aligned, lock_flags)
                                : com(volume, 0x24, locked, static_cast<Box*>(nullptr), lock_flags);
    if (result < 0)
        return result;
    if (lock_box) {
        box.left -= aligned.left;
        box.right -= aligned.left;
        box.top -= aligned.top;
        box.bottom -= aligned.top;
        box.front -= aligned.front;
        box.back -= aligned.front;
    }
    set(out, 0, locked[2]);
    set(out, 0x44, 0);
    set(out, 0x48, 0);
    set(out, 4, desc.format);
    set(out, 8, locked[0]);
    set(out, 0xc, locked[1]);
    set(out, 0x10, 0);
    set(out, 0x14, 0);
    set(out, 0x20, 0);
    set(out, 0x24, desc.depth);
    copy_words(static_cast<unsigned char*>(out) + 0x28, &box, 6);
    set(out, 0x4c, extra);
    set(out, 0x18, desc.width);
    set(out, 0x50, cookie);
    set(out, 0x1c, desc.height);
    set(out, 0x40, 1);
    set(self, 0, volume);
    com(volume, 4);
    return 0;
}

// 0x005e1e07: transfer from memory into a volume (callee-cleaned, 11 words).
extern "C" i32 __attribute__((stdcall))
FUN_005e1e07(u32 volume, u32 palette, u32 box, u32 memory, u32 format, u32 row_pitch,
             u32 slice_pitch, u32 source_palette, u32 source_box, u32 filter, u32 color_key)
    __asm__("_FUN_005e1e07");
extern "C" i32 __attribute__((stdcall))
FUN_005e1e07(u32 volume, u32 palette, u32 box, u32 memory, u32 format, u32 row_pitch,
             u32 slice_pitch, u32 source_palette, u32 source_box, u32 filter, u32 color_key) {
    unsigned char owner[12];
    FUN_005ec9be(owner);
    u32 holder;
    FUN_005f04e1(&holder);
    i32 result;
    if (volume == 0 || memory == 0 || source_box == 0)
        result = invalid_call;
    else {
        if (filter == 0xffffffff)
            filter = 0x80004;
        unsigned char destination[0x54];
        result = FUN_005f0a04(&holder, destination, volume, palette,
                              reinterpret_cast<const Box*>(box), 0, 0);
        if (result >= 0) {
            // Source descriptor; +0x40 and +0x44 are left as the original
            // leaves them, uninitialised.
            unsigned char source[0x54];
            set(source, 4, format);
            set(source, 8, row_pitch);
            set(source, 0xc, slice_pitch);
            set(source, 0, memory);
            copy_words(source + 0x10, ptr(source_box), 6);
            copy_words(source + 0x28, ptr(source_box), 6);
            set(source, 0x48, at(destination, 0x48));
            set(source, 0x4c, color_key);
            set(source, 0x50, source_palette);
            result = FUN_005f01ff(owner, destination, source, filter);
            if (result >= 0)
                result = 0;
        }
    }
    FUN_005f09ff(&holder);
    FUN_005ec9c8(owner);
    return result;
}

// 0x005e2f8c: transfer from one volume into another.
extern "C" i32 __attribute__((stdcall))
FUN_005e2f8c(u32 destination, u32 palette, u32 box, u32 source, u32 source_palette,
             u32 source_box, u32 filter, u32 color_key) {
    u32 holder;
    FUN_005f04e1(&holder);
    i32 result = 0;
    if (destination == 0 || source == 0)
        result = invalid_call;
    else {
        unsigned char desc[0x54];
        const i32 locked =
            FUN_005f0a04(&holder, desc, source, source_palette, reinterpret_cast<const Box*>(source_box),
                         0, destination == source ? 0x10001 : 1);
        if (locked < 0)
            result = locked;
        else {
            const i32 transferred =
                FUN_005e1e07(destination, palette, box, at(desc, 0), at(desc, 4), at(desc, 8),
                             at(desc, 0xc), source_palette, reinterpret_cast<u32>(desc + 0x28), filter,
                             color_key);
            if (transferred < 0)
                result = transferred;
        }
    }
    FUN_005f09ff(&holder);
    return result;
}
