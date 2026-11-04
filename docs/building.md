# Building and running

Run these commands from the project root with Zig 0.16.0 on your `PATH`.
Zig supplies the C++ compiler, Windows headers and import libraries, resource
compiler and linker. Python, mise, Visual Studio and a separate Windows SDK
are not required.

## Build and check

```powershell
zig build
```

The default build installs the 32-bit Windows GUI client to
`zig-out/bin/game.exe`. It compiles the sources in `src/client`, generates the
IJL import library from its committed definition, and embeds the Windows
resources from the editable catalog and assets. No original executable,
prebuilt client objects or sibling checkout is needed.

```powershell
zig build check
zig build test
```

`check` compiles, links and configures the executable without installing or
launching it. `test` checks Windows resource serialization, PE compatibility,
the login fade's animation clock, camera timing and effect blending after
particle, shadow and clothing passes. It also verifies that dropped-item
flashes do not tint subsequent NPC and character draws, and checks client-version
overrides through the production packet sender and an in-process TCP loopback
connection.
The native tests require Windows. The rendering test uses D3D11 WARP and
`d3dcompiler_47.dll` without game assets or a visible window. These checks do not
exercise server interaction or gameplay.

The default `ReleaseFast` mode uses `-O2`. Other supported modes are selected with
`-Doptimize=Debug`, `-Doptimize=ReleaseSafe` or `-Doptimize=ReleaseSmall`.
The recovered pointer layouts and calling conventions require the
`x86-windows-gnu` executable target. Files ending in `_seh.cpp` compile separately
with the MSVC x86 exception ABI; the client supplies its recovered C/C++ runtime
implementations. Link-time optimization is disabled to preserve the exception
ABI and assembly references. See [`build.zig`](../build.zig) for the build definition.

## Runtime requirements

Running requires Windows, a Direct3D 11 device supporting feature level 10.0 or
later, `d3dcompiler_47.dll` for runtime shader compilation, compatible game data
and a compatible server. These runtime inputs are not needed to compile.

Place the game archives `data.sah` and `data.saf`, settings files and required
runtime DLLs, including `ijl15.dll`, in `assets/` or another local directory.
The source project does not redistribute game archives or include a server.

```powershell
zig build -Dassets=assets
zig build run -Dassets=assets
```

`-Dassets` copies archives, settings, DLLs and text files beside the installed
executable; it excludes executables. `assets/` and the installed copies in
`zig-out/` are ignored by version control. Use a different directory with
`-Dassets=path/to/data`.

The run step builds and installs the client, uses `zig-out/bin` as its working
directory, and supplies `start game` by default. To pass arguments explicitly:

```powershell
zig build run -Dassets=assets -- start game
```

The optional `SHAIYA_LOGIN_HOST` environment variable selects a strict
dotted-decimal IPv4 login address. Without a valid override, the recovered
endpoint remains in use. For a local server:

```powershell
$env:SHAIYA_LOGIN_HOST = "127.0.0.1"
zig build run -Dassets=assets
```

`SHAIYA_CLIENT_VERSION` overrides the unsigned 32-bit protocol version sent when
selecting a game server. Set it to the value required by that server, using
decimal or `0x`-prefixed hexadecimal. Leading zeros remain decimal unless the
`0x` prefix is present. Signs, whitespace and dotted versions such as `5.4` are
not accepted. Missing, empty, invalid, oversized or overflowing values retain
the recovered default, `0x11122200`. The connection log uses the same value.

```powershell
$env:SHAIYA_CLIENT_VERSION = "0x11122200"
zig build run -Dassets=assets
```

## Optional mise setup

[`mise.toml`](../mise.toml) pins Zig 0.16.0 and provides two task shortcuts:

```powershell
mise trust
mise install
mise run build-client
mise run run-client
```

`build-client` runs `zig build`. `run-client` runs `zig build run -Dassets=assets`
with `SHAIYA_LOGIN_HOST=127.0.0.1`, so it builds, stages local assets and connects
to the login server on localhost. Supply the runtime files in `assets/` and start
a compatible local server before running it.

Both overrides can be set in a mise task's `env` table. For example:

```toml
[tasks.run-client]
env = { SHAIYA_LOGIN_HOST = "127.0.0.1", SHAIYA_CLIENT_VERSION = "0x11122200" }
run = "zig build run -Dassets=assets"
```

## Editor and source documentation

Open [`shaiya-game-decomp.code-workspace`](../shaiya-game-decomp.code-workspace)
or the project directory. VS Code tasks invoke `zig` from `PATH` for build,
check, test and run. Editor extensions are optional.

- [Client source](../src/client/README.md): modules and recovery boundaries.
- [Graphics](../src/client/graphics/README.md): native rendering and visual differences.
- [Windows resources](../src/client/resources/README.md): editing embedded assets.
- [Historical recovery notes](history/README.md): earlier validation and workspace records.

## Native 64-bit CMake build

CMake builds an integrated login/character application, migrated components and
an asset viewer. World loading and gameplay are not complete.
It rejects a 32-bit toolchain. Required: C++17 compiler, CMake, SDL2 development
files, OpenGL headers/library, OpenSSL 3.x and FreeType development files. CMake presets
also require Ninja and CMake 3.21+.
Zig and the original runtime DLLs are not dependencies of the native targets.

```sh
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug
```

Without presets: `cmake -S . -B build-native`, `cmake --build build-native`,
`ctest --test-dir build-native --output-on-failure`.

Run `build-native/genesis_native --assets assets --login-host 127.0.0.1` against
a compatible server. Defaults are login port 30800 and world port 30810;
`--login-port` and `--world-port` override them. `SHAIYA_LOGIN_HOST` and
`SHAIYA_CLIENT_VERSION` also apply (the version accepts decimal or `0x` hex).
Use `--frames 2 --capture new.ppm` for a finite hidden graphics smoke run.
Input supports Tab, Enter, backspace, Ctrl+A and Ctrl+V. Save ID stores only the
account name in `$XDG_CONFIG_HOME/genesis/native-login-id.txt` (or
`~/.config/genesis/`), or `%APPDATA%/Genesis/native-login-id.txt` on Windows.
Unchecking it removes that file. Passwords are masked and are never persisted.
Character selection currently ends on the loading screen. The application test
uses isolated synthetic servers and does not submit real credentials.
The selection screen previews characters using the original profile, face/hair
appearance tables and normal ANI action. Helmet and four body-clothing slots
resolve Item.SData Model directly through the recovered appearance consumer;
helmets replace hair (the original default is enabled). Weapon/shield attachments,
cape variants, height scaling and world entry/gameplay are still pending.
Characters with those unsupported attachments currently have no 3D preview.

`genesis_data` reads the original client SData envelope (SEED ECB using the
recovered expanded schedule, plaintext size and CRC32) and Item.SData records.
It preserves all 81 disk bytes and the original name/description bytes instead
of copying the x86 92-byte runtime structure. Lookups use one-based type/id.
`native_asset_item` validates all 100 groups and 22,073 items in the local archive;
`native_item` checks bounded parsing and equipment lookup without game assets.
The application caches this catalog and uses it to resolve helmet, torso,
trousers, gloves and boots; missing items retain the recovered default appearance.
The appearance consumer does not re-check equipment eligibility by class or slot.
Render tests exercise all 16 profiles with and without those equipment selectors;
the synthetic application peer also supplies equipped type/id bytes.

Graphics tests need a graphical session and OpenGL 4.5 core. `ctest --test-dir
build-native -LE graphics --output-on-failure` runs only CPU checks. Local game
archives enable two additional asset rendering smoke tests; CI does not require
or redistribute those archives.

```sh
build-native/genesis_assets --texture data/interface/login.tga
build-native/genesis_assets --model data/Entity/Building/Login_mainstructure.SMOD
build-native/genesis_assets --dungeon data/world/dungeon/DUN_LOGIN.dg
build-native/genesis_assets --character data/Character/Human/humf --animation data/Character/Human/ANI/humf_016_idle1.ANI
build-native/genesis_assets --skin data/Character/Human/3DC/humf_upper012.3DC --material data/Character/Human/DDS/humf_upper012.dds
build-native/genesis_assets --validate-textures
build-native/genesis_assets --validate-models
build-native/genesis_assets --validate-dungeons
build-native/genesis_assets --validate-skins
build-native/genesis_assets --validate-cloth
build-native/genesis_assets --validate-animations
build-native/genesis_assets --validate-appearances
build-native/genesis_assets --validate-actions
build-native/shaiya_archive assets --list
build-native/shaiya_archive assets data/Cash.SData
```

The viewer defaults to `assets/`; `--assets <directory>` selects another input.
`--skin` displays the authored bind pose of a standard character 3DC. Material
selection is explicit because its DDS name is not stored in that mesh format.
Add `--animation data/Character/Human/ANI/humf_016_idle1.ANI` to apply its
skeleton animation. The viewer loops at a 30 fps preview rate; the gameplay
animation clock is still pending. `--animation-frame 12` selects an exact frame
for repeatable captures, including with `--frames 2`.
`--character <race-directory/prefix>` loads six appearance-zero selections from
the original ML2 tables. It shares the sampled skeleton while retaining each
part's own inverse bind and material. Actual inventory/character-slot appearance
selection, helmets, weapons and gameplay scene ownership are still pending.
Use `--character data/Character/Human/humf --action 16` to resolve the original
slot through `humf_action.alt`, without choosing an ANI filename manually.
Empty action slots are rejected and later slots keep their authored indices.
The viewer still uses its preview clock; this option does not implement the
table's end modes or gameplay action transitions.
For a deterministic transition-pose preview, add
`--blend-animation data/Character/Human/ANI/humf_001_walk.ANI --blend-phase 2.5`.
Phases zero through five mix the sampled source toward the target's local
rest. This tool option does not drive the actor's request/commit clock.
`--validate-skins` reports unsupported variants too; the local archive currently
has 3,242 accepted skin files and 63 outside that contract. Of those, 48 are
known dynamic cloth meshes, decoded separately by `--validate-cloth`; the other
15 monster/NPC/strip variants remain pending. `--validate-cloth` follows the
original PC catalog references rather than treating cloth as skeleton matrices:
128 catalogs, 48 dynamic meshes and 384 static skinned meshes pass in the local
archive. Dynamic meshes preserve the embedded byte-string name, two opaque
vertex attributes, normals, UVs and topology. The 0x20c-byte simulation records
now decode into typed mesh/texture selectors, signed static-mesh sentinel,
grid dimensions, four owner parameters, a preserved reserved word and twenty
bone anchors. Anchor control uses only the low byte and stops at the first
disabled record, as in 00415e33/00416000; signed-negative vertex/bone entries are
skipped. `cloth_pins` transforms offsets by final bone matrices, independently
of inverse-bind skin palettes. All 3,072 setups agree with their mesh grids;
15,360 pins move between original normal-action frames zero and twelve.
`ClothParticles` now owns the portable particle grid and implements original
00580e30 mass initialization, 00581770 semi-implicit Euler integration and
005819d0 reset behavior. It preserves asymmetric boundary masses, pinned
velocity/acceleration, UVs and float-store boundaries. Both modes match checked-in
x86 reference bits for mass, position, velocity and acceleration after two steps.
All 3,072 local setups initialize successfully and run environmental steps with
animated bone pins. `external_forces` ports the environmental stage of 00582d90:
it clears prior forces, leaves pinned force zero, then applies gravity (-100),
quadratic drag (0.01) and scene wind direction times strength. It preserves the
00582540 near-zero normalization rules and intermediate float stores. Both
modes match the original x86 force bits with zero, positive and negative wind,
and a velocity near the normalization threshold. `spring_forces` now ports the
spring stage of 00582d90, using the ordered directed topology from 00581190 and
its strict predecessor-boundary rules. Mode zero uses stiffness 600/damping 2;
mode one uses stiffness 500/damping 1. The original 3x2 reference grid produces
42 springs; all endpoints, ordering, coefficients, rest lengths and resulting
force bits match the x86 reference in both modes. The original zero-length
spring propagates NaNs while returning success; the portable stage preserves
that IEEE/status behavior, checked by the reference rather than assumed.
All 3,072 local setups run five pinned, environmental and spring steps with
finite positions. `respond_contacts` ports 00581890: ordered velocity responses
use tangential factor 0.5 and normal factor -0.35, preserving intermediate float
stores. It skips inactive contact slots, processes repeated vertices sequentially,
and does not skip pinned particles or normalize supplied normals. The independent
x86 fixture covers axis-aligned, oblique, non-unit and zero normals, repeated
contacts and a moving pinned particle; resulting velocity bits match in both
modes. Position, force and acceleration remain unchanged during response.
`cloth_body` ports 00583370's shape preparation for all sixteen actor kinds.
It transforms four bone centers by the actor/world matrix and retains authored
radii (including zero), independently of world scaling. Actor kind selection
follows 004103a0's actual race jump table, with Vile in the last numeric block.
Centers and radii match the independent original x86 fixture using an affine
world transform, and all sixteen local normal animations move their body shapes.
`detect_contacts` now ports 00583370's particle traversal: it skips pinned
particles, clamps positions below the floor to floor+0.005, ignores exact sphere
centers, and tests spheres in reverse order. Near contacts use clearance <0.01;
penetration uses clearance <-0.0001. Phases one and two skip traversal. The
portable result retains the shared record overwrite rules, including the fourth
sphere's omitted penetration-counter increment. Records carry a normal with
radius -1 for near contact, or a center/radius for penetration; result status is
0, 1 or -1 respectively. All positions, statuses and record bits match 42 original
x86 cases covering boundaries, overlap, zero radii, exact centers, pins and phase
gates. Sixteen original animated cloth profiles also run a force/integration/
detection step and produce 231 finite, bounded collision records in the local
archive. `correct_penetrations` now ports the correction loops in 005845b0 and
00584853. It processes the penetration prefix until radius -1, using the supplied
global direction and absolute radius/distance gap. The scaled path multiplies
that gap by 1.5; the unscaled path uses one, with each path's original float-store
boundaries. It preserves repeated and pinned record behavior and does not
normalize direction or clamp outside-sphere gaps. Eight independent x86 cases
match resulting position bits; both paths also retain finite positions on the
local animated profiles.

`step` now integrates the complete 005843e0 and 005849a0 solver paths, including
the 005847c0 basic step, vertical row limits, contact response, penetration
correction and committed-position updates. Mode zero keeps the original
pre-constraint committed snapshot; mode one also applies parent-row forces and
reduces excessive separation. `advance` ports 00584ba0: phases zero/one run,
phase two resets, and other phases do nothing. The stored float delta times 200
determines a loop that always executes at least once, using fixed 0.0005 steps.
Mode one executes three solver passes per substep even after success; failures
reset before the next pass. Mode zero executes one pass. Twenty-two independent
original x86 frame cases match substep/attempt/reset counts and all position,
velocity, acceleration, force, pin and committed-position bits. The fixture
covers zero delta, float loop boundaries, phases, contact, row projection,
excessive separation and failure resets. All sixteen original animated cloth
profiles remain finite over ten native simulation frames. The application now
displays equipped capes using animated column deformation. Integration of the
full physical solver with the actor's world collision/wind scene remains pending.

`cloth_correction_direction` now follows 00416197..00416267: compose bone four's
final matrix with the actor world matrix, clear translation, transform local
positive Z, normalize and negate. The recovered bone record is 0x118 bytes and
its final matrix starts at 0xc0, so the original 0x520 access selects bone four.
The standalone cloth render test now derives its correction direction from the
animated skeleton instead of using a fixed negative-Z vector. Direction tests
cover axis/sign, translation independence, normalization, bone selection and
invalid matrix palettes. Bit-for-bit equivalence of the normalization dispatch
has not been established; the existing independent x86 solver fixtures retain
their original explicit input vectors.

`cloth_scene` prepares collision spheres, correction direction and wind from
the same skeleton/world inputs. The original 0041626d..0041629b wind setter
uses the correction direction and a 15-bit random sample multiplied by float
constants 0x38000100 and 0x3dcccccd, with one final float store. The portable
builder accepts that sample explicitly so callers can preserve RNG ordering;
it does not introduce a platform-dependent random generator. Tests check zero,
one, midpoint and maximum samples. The graphical cloth probe now uses this
builder. The ground height remains an explicit input: original 00410580 queries
the outdoor terrain by X/Z, or dispatches to dungeon collision with X/Y/Z.
Those world queries and the actor's RNG scheduling remain to be integrated.

The first outdoor height-query primitive is now portable:
`terrain_triangle_height` reproduces original 005b3640's plane interpolation at
X/Z, including its asymmetric float stores and normalized plane calculation.
Thirty-two independent original x86 instruction cases match the returned float
bits, with the exact input bits stored in `tests/fixtures/terrain_x86.h`.
This routine evaluates the plane; it does not select or bound a triangle.
`TerrainHeights` now reproduces 005b3360's unsigned 16-bit sample lookup.
Coordinates clamp to zero/extent inclusive, while the address remains
`extent * z + x`, not `(extent + 1) * z + x`. Original 005b3250 loads exactly
`extent * extent` 16-bit samples; the portable owner accepts that size and
checks the final address, including inclusive edge accesses, before reading.
Sixty-four original
x86 cases verify negative coordinates, row boundaries, the inclusive edge and
signed integer extremes. The generator below regenerates both reference sets.
The portable `decode_terrain_grid` reads the two consecutive blocks consumed by
005b3250: N×N little-endian 16-bit heights followed by N×N opaque bytes. It
reports how many bytes were consumed so the surrounding map parser can continue.
Original 005a34f0 uses the second block as an index into the surface records
and returns each record's sound name at runtime offset 0x338. This establishes
the surface-sound lookup, not the visual texture assignment or blending.
The native terrain drawing helper currently renders the four-triangle height
query fan with a caller-supplied texture. This is useful for validating height
data and OpenGL integration, but it is not evidence that the original terrain
renderer uses the same topology, UVs, texture assignment or level of detail.
The recovered visual paths `FUN_005b1710` and `FUN_005b18d0` in
`native_terrain_ports.cpp` iterate up to eight active layers. They submit
per-layer geometry ranges with base and optional secondary textures; one path
also supplies a detail texture, while the other can draw a separate multiply
detail pass. These paths and the corresponding 28/36-byte vertex layouts are
the reference for replacing the current single-texture diagnostic draw.
Original 005a2df0 calls this block reader before 005b2be0 on the same input
stream. Its generated field resource path uses the `Field` subdirectory; the
outer stream header and full resource selection still require reconstruction.
The archive also contains numbered `data/world/*.wld` resources. The local
`data/world/0.wld` is 3,783,087 bytes and begins with `FLD\0 00 08 00 00`;
the interpretation of the following value and its relationship to the terrain
sample dimension are not yet established. Do not infer the grid offset or
compression from this prefix alone.
The asset test currently identifies 33 `FLD` and 58 `DUN` world resources in
the installed archive and verifies each eight-byte `FLD` prefix. The original
loader reads the separate 256-byte block on the `DUN` branch, not on the `FLD`
branch. The point where the field height grid begins still requires validation.
For local world 0, the candidate grid ends at byte 3,151,883; the next
little-endian word is 6 and the following bytes begin `R1_Base.tga`. Original
005b2be0 reads a count at this stage and then a 256-byte name. This observation
provides a concrete starting point for reconstructing the material records; it
does not establish all record widths or their relationship to terrain cells.
`TerrainHeights::height` now ports 005b3720's selection of four triangles around
the center of each 4x4 cell. The two diagonal comparisons retain their original
strict boundaries. A separate fixture executes the complete original selector,
sample lookup and plane evaluator on irregular 16-bit heights: 1,156 queries
across four cells match native float bits, including both diagonals, cell edges
and out-of-bounds queries. The portable caller currently supplies the already
converted integer outside-height value explicitly. `world_height` also ports the
005a2c30 scaling and fallback conversion; 1,369 original x86 queries verify its
float results. Loading map sample data and dungeon ray queries remain pending.
The reference's `_ftol` shim truncates and pops ST0 while
preserving the caller's x87 control word; original selector instructions are
otherwise unchanged.
The world-height path preserves the extended intermediate through its world
height multiply/add; the public result is then stored as a float.
Startup 004443fb passes horizontal scale 2, vertical scale 0.02 and height
offset -200 to 005aef30. `default_terrain_scale` preserves these startup values
and the stored reciprocal calculations. Map loading is still required to
determine whether and when individual maps replace these defaults.
To regenerate the independent primitive reference with standard tools:

```sh
python tools/generate-terrain-reference.py /tmp/genesis-terrain-reference
i686-w64-mingw32-g++ -std=c++17 -O0 -static /tmp/genesis-terrain-reference/terrain_x86.cpp /tmp/genesis-terrain-reference/terrain.s -o /tmp/genesis-terrain-reference/reference.exe
wine /tmp/genesis-terrain-reference/reference.exe
```

`ClothResource` now draws an explicitly selected original cloth mesh and DDS
texture through the OpenGL 4.5 world/surface passes. Like original 00581b30, its
24-byte vertices use committed positions, white color and particle UVs, without
lighting normals. The material uses alpha blending, the original 7.5/255 alpha
cutoff, depth writes and no culling. The 00581d70 fallback translates every
authored row by its column's current first-row displacement. Optional emblem
sampling is not yet supported. CPU tests compare uploaded position bits with
the independent x86 solver fixture and check fallback column translation.

`native_asset_cloth_render` loads a real Human cape, advances thirty animated
frames and verifies visible, changing pixels for the committed and fallback
paths with no OpenGL error. It runs on Linux and the Windows x64 executable has
also passed under Wine; this is not a test on a physical Windows installation.
For reviewable captures, supply a new output prefix:

```sh
build-native/native_cloth_render_test assets /tmp/genesis-cloth-review
```

This writes `-rest.ppm`, `-simulated.ppm` and `-fallback.ppm` without overwriting
existing files. The test now chooses its texture from the setup's index in the
Human CTL catalog. The four original texture loaders (00584d50/00584e80/
00584fb0/005850e0) read a positive signed count followed by 256-byte names.
`decode_cloth_textures` preserves the original name bytes and checks record
bounds. `cloth_texture_paths` follows 00414884's actual race dispatch to HU,
EL, VI and DE; file names retain the original `CLOTH_TEXTHRE_*.CTL` spelling.
Asset tests resolve all 96 texture references and check the texture indexes
of all 3,072 PC setups against their racial catalog.

The portable equipment selector now also reproduces 004145a8..00414617:
cape type slot seven enables the cape, its Item.SData model selects the setup,
and upper-body slot one selects a catalog variant. The upper item's runtime
0x22 low byte comes from disk payload byte 28 (00461141..00461145), after two
earlier disk fields overwrite runtime offset eight. Subtract one with byte
wrapping and keep only results zero through seven; other values select zero.
A missing cape item retains model zero when its type is nonzero. Hidden capes
and zero cape type select nothing. `character_cloth_catalog` follows 00585b40's
gender/archetype branches and produces the one-based filename prefix. Tests
cover every variant byte and resolve all 128 original PC profile/variant files.
The application preview now instantiates the selected dynamic cape and optional
static skin mesh with that texture. Both use the character's world transform
and skeleton sample. The dynamic mesh follows 00581d70's animated column path;
the preview does not yet supply the full solver's actor scene. Slots five/six
(weapons/shields) still prevent preview loading; cape slot seven no longer does.
The graphics test compares rear views with/without capes for all sixteen
profiles and checks animated frames. An optional second argument to
`native_character_preview_test` writes a new PPM rear-view capture of its first
equipped profile without overwriting an existing file.

To regenerate the full-solver/frame reference independently:

```sh
python tools/generate-cloth-simulation-reference.py /tmp/genesis-cloth-simulation-reference
i686-w64-mingw32-g++ -std=c++17 -O0 -static /tmp/genesis-cloth-simulation-reference/cloth_simulation_x86.cpp /tmp/genesis-cloth-simulation-reference/simulation.s -o /tmp/genesis-cloth-simulation-reference/reference.exe
wine /tmp/genesis-cloth-simulation-reference/reference.exe
```

This probe initializes fixture owners and rest-limit inputs, executes original
solver/detector/frame instruction bodies, and uses transparent call counters to
observe frame dispatch. The matrix helper is scalar row-vector multiplication.
Native tests use the checked-in outputs and need no x86 compiler or Wine.

To regenerate the independent particle reference with a standard x86 compiler:

```sh
python tools/generate-cloth-reference.py /tmp/genesis-cloth-reference
i686-w64-mingw32-g++ -std=c++17 -O0 -static /tmp/genesis-cloth-reference/cloth_particles_x86.cpp /tmp/genesis-cloth-reference/particles.s -o /tmp/genesis-cloth-reference/reference.exe
wine /tmp/genesis-cloth-reference/reference.exe
```

The generator extracts particle initialization, integration, normalization and
force, spring-count, topology and contact-response instruction bodies, plus the
original penetration-correction loops wrapped with scratch storage. It refuses
overwrites. Native CMake builds use the checked-in fixtures and require neither Wine nor an
x86 compiler to run the CPU particle test.

The body/contact reference executes the original detector with a scalar
row-vector matrix helper. Shape cases omit traversal; detection cases capture
exact input floats and original output positions/records. To regenerate it:

```sh
python tools/generate-cloth-body-reference.py /tmp/genesis-cloth-body-reference
i686-w64-mingw32-g++ -std=c++17 -O0 -static /tmp/genesis-cloth-body-reference/cloth_body_x86.cpp /tmp/genesis-cloth-body-reference/detector.s -o /tmp/genesis-cloth-body-reference/reference.exe
wine /tmp/genesis-cloth-body-reference/reference.exe
```

The optional x86 animation reference is independent of the native build. To
regenerate its output using a standard 32-bit Windows GCC cross-compiler:

```sh
python tools/generate-animation-reference.py /tmp/genesis-animation-reference
i686-w64-mingw32-g++ -std=c++17 -O0 /tmp/genesis-animation-reference/animation_pose_x86.cpp /tmp/genesis-animation-reference/pose.s /tmp/genesis-animation-reference/matrix.cpp /tmp/genesis-animation-reference/quaternion.cpp /tmp/genesis-animation-reference/from_matrix.cpp -o /tmp/genesis-animation-reference/reference.exe
wine /tmp/genesis-animation-reference/reference.exe
```

The generator extracts original instruction/scalar routines from `src/client`
and refuses to overwrite existing generated files. Its fixtures are checked in
for native tests; this reference executable is not distributed with the game.
Model controls: A/D rotate, W/S elevate, wheel zoom, Escape quit. `--frames 2`
uses a hidden window for finite smoke runs. `--capture new.ppm` writes a top-left
RGB capture and refuses to overwrite a file. The archive tool accepts an optional
third argument to extract a resource to a new file. Archive lookup preserves
legacy byte hashing and resolves duplicate hashes to the last record.

### Windows x86-64 from Linux, without Zig

Install GCC/MinGW-w64 (Arch: `mingw-w64-gcc`) and obtain the official
[SDL2 2.32.10 MinGW development archive](https://github.com/libsdl-org/SDL/releases/tag/release-2.32.10).
The network component also needs a Windows x86-64 OpenSSL library. To prepare
SDL2, OpenSSL and FreeType with pinned SHA-256 archives and a standard GCC/Make build:

```sh
tools/prepare-mingw-deps.sh
```

This requires curl, tar, Perl, Make, CMake, Ninja, sha256sum and GCC/MinGW, and writes only
inside `.tools/`. The OpenSSL source archive is about 53 MiB and its initial
cross-build takes several minutes. Then:

```sh
cmake --preset windows-mingw
cmake --build --preset windows-mingw
```

The output in `build-windows` is PE32+ x86-64. The build copies `SDL2.dll` beside
the viewer. GCC runtimes are linked statically. No IJL or zlib DLL is needed for
the native asset viewer. The networking probe links OpenSSL statically. Other SDL2 locations can be selected with `-DSDL2_DIR`.
To run the finite renderer test under Wine: `wine build-windows/native_renderer_test.exe`.
Wine checks do not replace validation on native Windows hardware.

### Build on Windows

Use a 64-bit Visual Studio C++ toolchain and SDL2 (for example vcpkg's
`sdl2:x64-windows`, `openssl:x64-windows` and `freetype:x64-windows` with its CMake toolchain), then:

```powershell
cmake -S . -B build-windows -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build-windows --config Release
ctest --test-dir build-windows -C Release --output-on-failure
```

An OpenGL 4.5 driver is required for graphics tests. Hosted Windows CI runs CPU
tests because its default driver does not provide that context. Native MSVC/CI
execution has not yet been verified locally. See [native-port.md](native-port.md)
for migration scope and remaining integration work.

## Native network checks

```sh
ctest --test-dir build-native -R 'native_login|native_wire|native_tcp' --output-on-failure
build-native/genesis_login_probe 127.0.0.1 30800
```

The probe needs an existing compatible server and only exchanges A101; it does
not submit credentials. CTest uses an isolated loopback fixture. Authentication
and server selection are checked with synthetic packets, not a real account.
See [native-network.md](native-network.md) for protocol fidelity and limits.

## Native text checks

FreeType rasterizes a system font; OpenGL owns the glyph atlas. `GENESIS_FONT`
selects a TrueType/OpenType file. Default searches include Arial/Liberation /
DejaVu/Adwaita on Linux and `%WINDIR%/Fonts/arial.ttf` on Windows. No system font
file is redistributed.

```sh
ctest --test-dir build-native -R 'native_font|native_text' --output-on-failure
```

The font test is CPU-only. The text test requires OpenGL 4.5 and checks glyph
pixels, cache reuse and texture lifetime. Complex shaping and full GDI layout
parity are not established. See [third-party notices](../licenses/README.md).
