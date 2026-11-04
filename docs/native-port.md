# Native 64-bit port

Final graphics requirement: **OpenGL 4.5 only on Linux and Windows**. DX11 is
removed from the final client, not retained as an optional backend.

The playable recovered client remains Windows x86 + DX11 in `src/client`.
The CMake targets include an integrated application and an asset viewer, **not a
playable replacement game client yet**. Do not remove `build.zig` or redirect the game
launcher until the native application supports the client behavior.

## Implemented and exercised

- C++17/CMake, GCC on Linux and GCC/MinGW x86-64 for Windows; no Zig in this path.
- SDL2 window/context lifecycle and input, OpenGL 4.5 core, context-local function
  loading, resize, depth/stencil attachments and presentation.
- Persistent vertex/index buffers with retained CPU geometry and 16/32-bit GPU
  indices; native tests exercise indices above 65,535 and range/base-vertex bounds.
- Nested offscreen targets, with scene, viewport size and scissor restoration;
  pixel tests verify render-texture orientation and nested target contents.
- Shared fixed-width vertex/scene definitions in `graphics/render_types.h`.
  Runtime resource ownership uses host pointers and containers.
- GLSL versions of the 18 vertex and 22 pixel entry-point behaviors from
  `native_render_shaders.h`: UI, lightmaps, static and skinned objects,
  material layers, projected shadows, reflective surfaces, water, glow, gamma.
  Shader compilation and synthetic GPU tests pass; full visual parity of all
  effects is not established. The game does not yet call these passes.
- Custom blend factors following `native_render_passes.cpp`'s `blend_state`/
  `alpha_factor`: `Style::custom_blend` carries an explicit source/destination
  `BlendFactor` pair (the subset the recovered client actually authors: water's
  constant-factor blend and the 13-entry actor-effect catalog table) plus the
  constant blend color, bypassing the named `Blend` enum exactly as the D3D11
  reference does when a draw's style sets non-zero source/destination factors.
  GPU pixel tests cover the constant-factor pair and a plain one/zero pair.
  No draw site requests it yet, matching the other shader passes above; it
  exists for water and actor-effect rendering once those are connected.
- Texture quality-policy algorithms from `load_texture`'s non-DDS-passthrough
  path: `texture_box_mip` (2x2 box-filtered mip, edge taps clamp),
  `texture_resize_triangle` (the original's separable triangle-filter resize,
  including its wrapped, not clamped, out-of-range sample addressing) and
  `texture_apply_color_key` (24-bit RGB match zeroes alpha). CPU tests cover
  mip averaging/clamping, that resizing a solid color reproduces it at
  several target sizes, that a two-column image actually blends instead of
  nearest-sampling, and color-key matching. `decode_texture_levels` assembles the complete `load_texture` policy on top
  of them (requested/power-of-two sizing, authored DDS levels versus generated
  box mips, `level_limit`, per-level color key, triangle resize) and
  `texture_quality_options` ports `quality_options()`. `Renderer::load_texture`
  uploads the resulting chain through `texture_levels` with no GPU mip
  generation; cloth textures use it. Other call sites still use
  `texture_rgba` (GPU `glGenerateMipmap` or one level) until their authored
  options are traced.
- `GlowEffect` ports the `native_postfx_ports.cpp` orchestration: capture,
  8-tap half-size downsample, up to 16 gaussian x/y iterations, additive
  composite with the original strength rule. `ShadowProjection`/`ShadowMap`
  port the actor shadow camera, projective bias and offscreen pass scoping.
  `genesis_native --glow 1|2` runs `GlowEffect` between the 3D scene and the
  interface (default 0, matching the original's off state); the connected
  world's loading scene now stands the selected character on the terrain (true
  world units, third-person camera, ground height from `world_height`) and
  projects its top-down `ActorShadow` onto the terrain batches. The shadow is
  the original's small top-down silhouette, mostly hidden under the feet from
  this camera. Movement, collision and animation states are still absent.
- `water.h` ports the water/planar-reflection orchestration: the procedural
  R8G8 signed normal source, wrapped height-to-normal conversion, ±2048 quads,
  mode 0/1 animation, projective-bias capture of reflection/refraction cameras
  with white-cleared capture targets, `draw_water` (both modes) and the
  underwater attenuation draw, which is the first draw site of
  `Style::custom_blend`. GPU pixel tests cover all of them; no world scene
  instantiates water yet.
- `Text::draw_in_rect` ports DrawText layout (CRLF, centering, vertical
  alignment, CALCRECT, clipping) and `Text::outlined_label` ports the
  five-pass outlined label with `<<` segment markup and blue-key transparency.
  Complex shaping (the original's Uniscribe path) remains unported.
- `encode_png` writes lossless PNGs (no zlib) for captures; no live hotkey exists
  because the original's `capture_jpeg` caller is untranscribed assembly.
  `Renderer::resource_counts()` exposes live textures,
  meshes and targets so owners can verify unloading.
- CPU texture decoding from DDS BC1/2/3 and packed channels, TGA/RLE and BMP.
  No IJL, WIC or legacy DLL is used by these components.
- SAH/SAF lookup, listing, extraction and fixed-width offsets; portable SMOD
  geometry/material loading. Normals and UVs preserve authored NaN bits; vertex
  positions, reads, counts and indices are checked. Uninterpreted SMOD tail
  records are retained, not interpreted as native pointer layouts.
- DG dungeon decoding retains bounds, octree children, texture groups, all
  44-byte lightmapped vertices and node-local collision geometry in typed host
  containers. Reusable GPU resources load base textures/lightmaps and unload
  meshes/images on destruction. The viewer can render a complete DG; world
  placement, octree culling and collision queries are not connected yet.
- FLD outdoor terrain decoding retains the field's 16-bit height grid, its
  per-sample attribute bytes and the original material records following
  `005b3250`/`005aefb0`. `TerrainHeights` reproduces the recovered four-triangle
  interpolated query (`005b3720`) and the signed outside-height fallback used
  by `005a2c30`. `TerrainResource` batches cell meshes by their authored
  material index, owns one detail texture per batch and can center its
  rendered span on a requested world position instead of the field's middle.
  The viewer's `--terrain` option renders a centered span of a real field
  (bounded to stay under the existing region draw cap) on Linux and the
  Windows x86-64 build under Wine. `Application` owns one `TerrainResource`
  for the connected world's selected field, answers surface-sound queries
  from it and, once a character has entered a map with a resolved spawn
  position, renders that field centered on it behind the (still
  unimplemented) loading screen — this is not yet gameplay: no entities,
  movement or collision exist past this point. World-scale streaming and
  per-cell collision remain pending.
- Standard character 3DC decoding retains the inverse-bind matrices, authored
  40-byte skin vertices and 16-bit indices. It follows the two-influence
  offsets/weighting in `00550fb0`/`005514a0` and the native actor skin pass.
  Palette composition is inverse-bind times animated final in row-vector
  order. `SkinResource` owns GPU buffers/material and can draw the bind pose
  or consume final skeleton matrices. ANI poses now drive this path in the
  viewer. Six-part composite character ownership now shares one sampled pose
  across independently bound meshes; gameplay equipment selection and animation
  states are not implemented yet.
- ML2 appearance tables retain both name catalogs and 12-byte mapping rows
  following `005537d0` (`006cca18` is the original ML2 tag). `CharacterResource`
  resolves the authored mesh/DDS pair for each requested appearance, loads all
  parts with exception-safe ownership and uses common world coordinates. The
  viewer can assemble face, hair, upper, lower, hands and feet at appearance
  zero. Equipped-item appearance consumers are still pending. Reflective body parts use the actor reflection shader, where
  texture alpha controls reflection rather than surface transparency;
  environment/specular scene inputs and complete material parity remain pending.
- ANI decoding owns authored matrices, signed frame numbers, parent links and
  independent quaternion/translation keys without converting to another asset
  format. Negative start frames are preserved, matching signed frame loads in
  `00552910`; the bone-parent root test also accepts negative parent sentinels.
  Hierarchy cycles, out-of-range parents, truncated keys and non-finite data
  are rejected. `AnimationPose` prepares local rest matrices as raw times
  inverse raw parent, samples the separate channels and composes final parent
  transforms. The recovered two-frame key search, pre-first-key extrapolation,
  last rotation interval, quaternion shortest arc and static-bone fallback are
  retained. Transition pose blending and request/commit state now exist as
  portable components; actor gameplay ownership remains pending. The viewer's
  optional 30 fps preview clock is a tool setting, not
  evidence that the gameplay clock has been migrated.
- AL3 action tables retain sparse slot IDs, original ANI names, the full
  control word, next-action index and four raw parameter floats, following
  `00552120`. Mode queries use only the low control byte: real records have
  nonzero padding in its upper bytes. `--character ... --action N` resolves
  the authored ANI through this table. It selects content only; end modes,
  action locking, frame rates, impact events and transition blending are not
  implemented by this viewer option.
- `AnimationPose::blend_to_rest` ports `00552cb0`: source channels are sampled,
  local matrices are converted to quaternions, blended toward the destination's
  local rest using phase * 0.2 and recomposed with the source hierarchy. Target
  ANI keys are deliberately not used during this transition. Original x86
  translation and rotation results are captured as fixtures. The stateless
  component requires the target to cover source bones; the original mutable
  owner retains prior finals for unmatched bones, which is still pending.
- `ActionTransition` ports the `004105b0` request gates and `0041b3a6` clock:
  locks permit action nine, ordinary requests reject the current action or an
  already-active blend, and the earlier special handler's result is supplied
  by the caller. Transition phase advances by delta * 30, freezing the source
  frame and committing only when the unrounded phase exceeds five. Commit
  resets the animation frame to zero. End modes, special handler behavior,
  combat impact timing and the actor owner still need integration.
- Recovered AES/SHA-256/LCG/transport crypto and animation timing components.
- Native nonblocking POSIX/Winsock TCP with per-connection framing and crypto;
  OpenSSL BIGNUM replaces x86 RSA arithmetic. Typed A101/A102/A201/A202 login
  and server-selection state is implemented and connected to the native UI.
  See [network contracts and limits](native-network.md).
- The connection owner transitions to the world socket, preserving/resetting
  crypto. Native character catalog, selection responses, faction, quick slots
  and AES/XOR switches are tested. Unported gameplay remains queued events.
- Selected-player details, 32-bit resources, money, combat counters, additional
  stats and map coordinates now have typed native ownership following the
  recovered client readers. This removes these packets from the unhandled
  queue; actor creation, map loading and gameplay consumers remain incomplete.
- FreeType font loading, UTF-8 decoding, metrics and a shared OpenGL glyph atlas
  replace GDI handles in the native text component. Tests cover pixel output,
  cache reuse, localized glyphs and texture updates/releases. Complex shaping
  and full legacy font layout/outline parity remain incomplete.
- `genesis_native` owns the SDL loop, input, login backdrop, interface textures,
  text and connection. It supports CP1252 account fields, masked passwords,
  authentication retry, server and character selection, and optional saved ID
  without a stored password. A two-server fixture exercises this entire UI path
  on Linux and the Windows build under Wine. Character selection currently
  stops at loading; playable world/gameplay is not implemented.
- The native character-selection screen now instantiates the selected
  unequipped character from its race, job, gender, face and hair. Profile
  selection follows `00417220`, the `0041760c` branch table, `00552400` and its
  actual `00552550` branch table, covering all 16 Human/Elf/Vile/DeathEater
  profiles. The normal AL3 action advances at 30 frames/second, keeps the final
  frame at equality and resets to zero after the end (discarding overshoot),
  following `0041be09`/`0041be75`. Resources are cached per selection and
  released when leaving the screen. Equipped characters still require Item.SData
  mapping and are not substituted with an invented unequipped loadout. Height
  scaling, full selection-scene materials/effects and gameplay actors remain
  incomplete.

The renderer keeps row-vector/row-major matrices and the existing half-pixel
adjustment. Its FBOs use `glClipControl(GL_UPPER_LEFT, GL_ZERO_TO_ONE)`; presentation
flips only the window blit, preserving top-left render-texture sampling.
See [Khronos clip control](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_clip_control.txt)
and [SDL function loading](https://wiki.libsdl.org/SDL2/SDL_GL_GetProcAddress).

## Local validation

On the development machine (NVIDIA OpenGL 4.5, Linux x86-64):

- Native CTest suite passes, including pixel checks for orientation, ARGB,
  alpha cutoff, blending, scissor, depth, row-major transforms, two-bone skinning,
  resize and presentation. Synthetic passes do not establish gameplay parity.
- CPU tests also pass with AddressSanitizer and UndefinedBehaviorSanitizer.
- The Windows viewer imports SDL2 and Windows CRT/system libraries, with no
  Direct3D imports. Both native targets use the same OpenGL renderer.
- All 11,898 DDS/TGA/BMP textures in local archives decode successfully on
  Linux and with the Windows x86-64 executable under Wine. Archive path
  utilities retain legacy code-page name bytes rather than passing virtual
  asset names through OS filesystem/locale conversions. CP1252-name regression
  checks run on both builds.
- 2,162 SMOD resources parse successfully. Two resources are rejected:
  `data/Entity/Tree/Gate_iii_S.SMOD` is truncated under the reviewed format and
  `data/world/dungeon/R1_Trade/R1_Trade.SMOD` contains an out-of-range mesh index.
  They need format/content investigation; no game asset was modified.
- The login main structure renders its 24 meshes and authored textures.
- All 51 local DG resources parse completely, also under ASan/UBSan. Tests
  reject every truncated prefix of a two-node fixture, invalid material/vertex
  indices and non-finite positions. Empty inverted octant bounds are preserved.
  `DUN_LOGIN.dg` renders 131 meshes with its four lightmap pages on Linux and
  the Windows x86-64 build under Wine. This does not establish full map parity.
  `Login_eternity_territory_01.dg` decodes but its four named lightmap pages are
  absent in the local archive; the strict GPU loader reports the missing path.
- Of 91 local `.wld` world descriptors, 33 carry an outdoor `FLD` field and all
  33 decode (height grid, attributes and material records) without failure;
  the other 58 link an indoor `DUN` dungeon instead and are skipped, not
  counted as failures, since they carry no field terrain to decode. `0.wld`
  renders as a centered, material-batched span on Linux and the Windows
  x86-64 build under Wine; this exercises one representative field, not every
  local map's geometry or textures.
- The standard skin decoder accepts 3,242 local 3DC files. Another 63 are
  rejected: 48 cloaks, three monster files, five NPC files and seven Strip
  files. These need their actual client loading contracts reviewed; they are
  not declared corrupt or silently coerced into standard character meshes.
  `humf_upper012.3DC` renders with its original DDS material on Linux and the
  Windows x86-64 executable under Wine. Skin CPU/GPU tests also pass under
  ASan/UBSan. Synthetic tests
  cover matrix order, two-bone blending, authored unused bone bytes, every
  truncated prefix, invalid indices/weights and short palettes. EP6 version
  444 is explicitly unsupported by this two-influence path.
- All 112 local ML2 tables decode completely. Appearance tests cover aliases,
  names, flags, both catalog indices and truncation. The composite character
  test renders the original human `humf` model with six mapped materials and
  verifies that ANI changes its pixels, a failed partial load leaves existing
  GPU resources intact, and armor is not made transparent by reflection alpha.
  Some parts contain extra unused inverse-bind matrices (the default torso has
  38 while idle uses 36 bones); only referenced bone indices require animated
  finals, matching the original per-vertex skinning accesses.
- The ANI decoder reads 3,711 local animations completely. Twenty others fail
  finite matrix/key validation and need a review of their actual loading
  contract. These are not reported as corrupt assets. Tests cover independent
  key channels, static bones, signed -1 frames, row-vector matrix storage,
  forward parents, cycles, missing parents, truncation and original human /
  monster clips. Decoding alone does not establish animation playback parity.
- Pose tests compare native root translation/quaternion results against an
  executed x86 reference extracted from the original `00552910`, `00552cb0`,
  `005e5adf`, `005e6789` and `005e6d56` routines. `tools/generate-animation-reference.py` reproduces
  the reference sources; CMake tests consume captured fixtures and do not
  require an x86 compiler or Zig. Tests also sample every integer frame of the
  original human idle clip and verify non-static output. The original armor
  piece renders with ANI poses; complete character/gameplay visual parity is
  still unproven.
- Windows x86-64 component executables build with GCC/MinGW and the CPU tests,
  renderer test and texture viewer execute successfully under Wine. Native
  Windows hardware execution and the newly added CI workflow remain unverified.
- The retained `zig build check` and `zig build test` pass (Windows tests use the
  host's Wine binfmt integration). This only validates the retained client.

Game data and generated captures are local inputs, not distributed source.
All 16 local AL3 tables decode completely and their 1,252 nonempty ANI references
exist in the archive. Tests preserve holes, padding and raw parameter bits,
reject truncation/version/name failures and exercise action selection through
the native OpenGL viewer.
Profile tests cover the recovered byte selector branches and original resource
sets. Graphics tests assemble and render all 16 base profiles, verify normal
action boundary/reset behavior and visible animation, and the application test
requires the selected character preview to load and unload with the UI state.

## Remaining migration

1. Rendering API: components exist and are tested; what remains is
   instantiating glow/shadow/water/texture policies from world and actor
   scenes (item 2), Uniscribe-equivalent shaping, and side-by-side frame
   comparison against DX11 once a scene can be drawn by both.
2. Port world/model/actor/animation loaders and scene ownership. Replace fixed
   offsets and packed pointer-bearing owners with typed host structures while
   retaining disk/network layouts.
3. Replace the remaining Win32 application/message loop, input, audio and file
   services and x86 custom CRT/SEH/assembly contracts. Native TCP/framing/login
   and world-handshake/character components exist; remaining gameplay protocol
   integration is still incomplete. Existing recovered code
   cannot simply be recompiled with 64-bit flags.
4. Complete world/gameplay integration after the connected login/character UI;
   exercise server interaction and game content on Linux and Windows x86-64.
5. Switch the launcher and remove Zig/legacy DX11 only after the replacement is
   playable and regression checks establish the required behavior.
