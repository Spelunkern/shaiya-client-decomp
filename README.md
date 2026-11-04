# shaiya-game-decomp

An AI-assisted reverse-engineered C++ client for Shaiya Episode 5.4. The project
combines recovered C++, x86 instruction transcriptions and newly authored code.

## Contributions and scope

This fork aims to migrate the client to 64-bit native builds for Linux and
Windows, and replace Direct3D 11 with OpenGL 4.5. A build without a Zig
dependency is preferred. These are migration goals, not completed features;
the current build and runtime requirements below still apply. Preserve
recovered client behavior while replacing platform-specific code.

## Native migration build (Linux / Windows x86-64)

The native components use C++17, CMake and SDL2 with OpenGL 4.5; Zig is not
required for these targets. `genesis_native` integrates login, server selection
and character selection with the original interface assets. **World loading and
playable gameplay are still incomplete**. An asset viewer is also provided.
The original client build is retained below.

```sh
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug
build-native/genesis_native --assets assets --login-host 127.0.0.1
build-native/genesis_assets --model data/Entity/Building/Login_mainstructure.SMOD
build-native/genesis_assets --character data/Character/Human/humf --animation data/Character/Human/ANI/humf_016_idle1.ANI
```

The viewer reads local `assets/data.sah` and `assets/data.saf`. Use A/D to rotate,
W/S to elevate and the mouse wheel to zoom. See the [native port status](docs/native-port.md)
and [build guide](docs/building.md) for the Windows cross-build and current limits.
`--dungeon data/world/dungeon/DUN_LOGIN.dg` renders DG geometry with lightmaps;
`--validate-dungeons` checks all archived DG files without a graphics context.
`--terrain data/world/0.wld` renders a centered span of an outdoor FLD field,
batched by material; `--validate-terrain` decodes every archived field.

## Retained game client build

Install Zig 0.16.0, put `zig` on your `PATH`, and run from this directory:

```powershell
zig build
```

Zig is the only required build tool. The output is `zig-out/bin/game.exe`, a
32-bit Windows executable. The original executable is not needed to build it.

To run on Windows, place compatible `data.sah`, `data.saf`, settings and runtime
DLLs (including `ijl15.dll`) in `assets/`, then run:

```powershell
zig build run -Dassets=assets
```

Alternatively, [mise](mise.toml) can install Zig and run the client against
localhost: `mise install`, then `mise run run-client`.

See [build and runtime details](docs/building.md) for requirements, checks and options.

## Differences from the original client

- **Rendering:** native Direct3D 11 replaces Direct3D 9, with newly authored
  shaders and rendering passes. Some visual effects may differ.
- **Background updates:** rendering continues while unfocused or minimized;
  animation timing retains fractional milliseconds at high frame rates.
- **Login camera:** the introductory pullback uses a stable timeline calibrated
  against the original D3D9 client, so rendering load does not change its speed.
- **Connection overrides:** `SHAIYA_LOGIN_HOST` selects the login server's IPv4
  address; `SHAIYA_CLIENT_VERSION` selects the numeric protocol version sent to
  the server. See the [build guide](docs/building.md) for mise task examples.

The native 64-bit selection preview now resolves helmets and body clothing from
the client's original Item.SData and appearance tables. Weapon attachments and
world gameplay are still pending. Equipped capes now appear on the selection
character with animated column deformation and their static mesh attachment.
Full cape physics runs in a separate asset test; its actor scene integration
remains pending. See the [build guide](docs/building.md).

This is an ongoing reconstruction. Complete visual and gameplay equivalence
with the original client has not been established.
