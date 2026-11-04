# Project guidance

This is an AI-assisted reverse-engineered Shaiya Episode 5.4 Windows C++ client.
Production sources live in `src/client`. The existing client is built with Zig
0.16.0 through `build.zig`. The fork targets a native 64-bit client on Linux and
Windows using OpenGL 4.5. A build system that does not require Zig is preferred;
retain the existing build until a replacement actually supports the client.

- The existing implementation depends on recovered 32-bit layouts, x86 calling
  conventions and exception behavior. Preserve its behavior while replacing
  those dependencies with portable representations; do not assume it can be
  made 64-bit by changing a compiler flag. Files ending in `_seh.cpp` use the
  MSVC x86 exception ABI; other units use the GNU Windows x86 ABI.
- Migrate rendering from Direct3D 11 to OpenGL 4.5 as part of the native port.
  Keep the existing renderer functional until its replacement is ready.
  The final client must use OpenGL 4.5 as its only graphics API on both Linux
  and Windows. Remove DX11 from the final client; it is a temporary reference,
  not an optional final backend.
- Keep current instructions in `README.md` and `docs/building.md`.
  `docs/history/` contains records of the previous workspace and toolchain.
- The recovered client is the source of truth for formats and behavior. The
  local server is only a basic login/testing aid; do not change client contracts
  to accommodate server discrepancies.

Use `zig build check` to check compilation and linking after C++ or build
changes. Use `zig build test` for resource serialization and PE build-support
changes. These tests do not validate rendering or gameplay; check those changes
with targeted runtime observations.
