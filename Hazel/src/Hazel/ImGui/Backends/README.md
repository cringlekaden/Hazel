# Dear ImGui Metal 4 renderer backport

Source: ocornut/imgui docking commit
`9dbcb2f1cc0e8a3ffac51d7f9d28efdb228db14b`, MIT licensed (see
`Hazel/vendor/imgui/LICENSE.txt`). The pinned ImGui core and GLFW/OpenGL backends
remain at `e25e4526cd41cd6536194de098540d54244f54e9` (1.89.4 WIP).

This backports the official renderer rather than upgrading the editor's core,
ImGuizmo, docking internals and all platform backends together. Compatibility:

- Static 1.89 font atlas upload replaces the 1.92 dynamic texture protocol.
- Legacy CmdListsCount/array iteration and ResetRenderState callback are retained.
- Native pointers use full-width ImTextureID at the ImGui boundary only. Hazel's
  EditorTexture captures an immutable resource owner; no uint32 Metal texture IDs.
- Per-texture sampler lookup preserves nearest/linear/mipmap/wrap settings from
  the captured Hazel version; fonts and encoded targets keep the default UI sampler.
  MetalDevice generates pending UI-only mip chains before consumption and marks
  them submitted only after commit. The superseded dynamic-texture wrapper is removed.
- Hazel MetalDevice owns surfaces, allocator slots, command buffers, one queue,
  residency of user images and the completion event after ALL window consumers.
  Upstream's secondary-window allocators/events/submission callbacks are removed.
- Each Hazel slot has its own backend residency set. Only a completed slot clears
  it; user texture generations remain owned/resident by Hazel until completion.
- Hazel's native Objective-C++ target compiles with ARC. The pinned Xcode
  generator omits file-filtered buildoptions, so Premake sets the native
  CLANG_ENABLE_OBJC_ARC target setting. MetalSurface transfers exactly one retain
  into the existing C++ layer owner; no parallel ownership system is introduced.

November 16 fixes included in the source assessment:

- `fb57aa8aa3b92720859aeeaeb09822679eb784b9` (#9621/#9624): per-slot event
  values. Hazel avoids that wait entirely: the production slot completion event
  covers every viewport before NewFrame can recycle its UI storage.
- `514f6a8e7f2deeab14063bf9850ddacaadde9aee` (#9622/#9625): defer vertex/index
  buffer recycling until NewFrame reuses a completed slot. Kept in this backport.
- `96704190661ed45686cc0850087d9e475d398ee9`: align each 16-bit index draw to
  four bytes. Kept.
- `15d4b457dcc831489484e92260e4324668ba0a6b`: preserve each viewport's reserved
  projection matrix during render-state reset. Kept.
- `2aa0ddb13dc749429e6fe448816ca501dee00abe` and
  `9dbcb2f1cc0e8a3ffac51d7f9d28efdb228db14b`: select secondary pass formats and
  make drawable resources resident. Hazel uses BGRA8 surfaces and explicitly
  attaches each layer residency set to the shared command buffer.

The unused Metal-C++/manual-autorelease wrapper, empty teardown and dead display-sync
state were removed; Hazel calls only the ARC Objective-C++ bridge. Native allocation
failures return failure or throw before nil resources enter arrays/encoding.

The unchanged upstream MSL now lives in `Hazel/Resources/shaders/ImGui.metal`.
The offline producer builds `ImGui.metallib`; runtime loads that library without
shader source compilation. Package notices retain the original ImGui MIT license.

Qualification and platform disclosures belong in `docs/design/metal-m5.md` and
`docs/design/metal-m6.md`. Upstream
fixes are necessary evidence, not proof that Hazel's integration passes.
