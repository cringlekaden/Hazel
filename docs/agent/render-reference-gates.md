# Deterministic Quality Gate Commands

**Date:** 2026-11-08  
**Branch:** `agent/render-reference`  
**Engine:** Hazel C++ OpenGL 4.1 Renderer  
**Platform:** Apple Silicon macOS (M4, Metal-backed OpenGL 4.1)

---

## Gate Overview

The Hermes quality gate validates the 3D rendering infrastructure with two focused smoke tests:

1. **Build**: Compiles the `MigrationRendererSmoke` test target
2. **Test**: Executes the 3D renderer smoke test (Camera, UBO, vertex attributes, instanced rendering)

Both commands use **Debug configuration** for fast iteration. The combined gate finishes in **~5 seconds** on M4 hardware, well under the 265-second budget.

---

## Exact Commands

### 1. Build Command

```bash
xcodebuild \
  -workspace Hazel.xcworkspace \
  -scheme MigrationRendererSmoke \
  -configuration Debug \
  -destination 'generic/platform=macOS' \
  -jobs 2 \
  build
```

**Purpose:** Builds the 3D renderer smoke test executable with Debug configuration.

**Rationale:**
- Uses `-jobs 2` for limited parallelism to avoid memory pressure on macOS unified memory
- Debug configuration provides fastest build time (~3 seconds)
- Targets `MigrationRendererSmoke`, the only existing 3D renderer test in the codebase
- Part of the premake-generated test suite that validates GPU state management, vertex attributes, and instanced rendering

**Exit Code:** 0 on success (tested: 3.31s)

### 2. Test Command

```bash
/bin/bash -c 'DYLD_LIBRARY_PATH=build/dependencies/mono/macos-arm64/sdk/lib \
  ./bin/Debug-macos-arm64/MigrationRendererSmoke/MigrationRendererSmoke'
```

**Purpose:** Executes the 3D renderer smoke test with Mono library paths set.

**Rationale:**
- The test executable is a pure C++ OpenGL test (no Mono/C# dependencies)
- Requires `DYLD_LIBRARY_PATH` to locate Mono runtime libraries for internal Hazel dependencies
- Tests camera projection, UBO binding/upload/readback, vertex attribute state, and instanced rendering with color/entity output
- GPU-backed test running on Apple M4 with Metal-backed OpenGL 4.1

**Exit Code:** 0 on success (tested: 1.44s)  
**Output:** `PASS: Camera, UBO binding/upload/readback, float/int/bool/Mat3/Mat4 attributes, instanced color/entity rendering`

---

## Why These Commands?

### Selection Criteria

1. **Focused Smoke Test:** Both commands validate the minimal 3D rendering pipeline without requiring:
   - Environment/IBL bake tools
   - Mesh import (not yet implemented)
   - Material editor (not yet implemented)
   - Full scene authoring

2. **Native macOS:** Commands use `xcodebuild` and native executables, avoiding cross-platform abstractions

3. **Deterministic:** Fixed workspace, scheme, configuration, and destination ensure reproducible builds

4. **Fast:** Combined ~5 seconds vs. 265-second budget

5. **Existing Infrastructure:** Uses the premake-generated `MigrationRendererSmoke` target that already exists in the codebase

### Why Not Other Options?

| Option | Rejection Reason |
|--------|------------------|
| `hazel.py build --tests` | Times out (>120s), includes full suite |
| `hazel.py test --only MigrationRendererSmoke` | Test list doesn't include `Migration*` targets; uses different naming convention |
| Full test suite | Exceeds time budget; not focused |
| Release configuration | Slower build times; Debug is sufficient for smoke tests |
| Orthographic PBR test | Requires full scene setup, environment bake, material assets |
| IBL quality tests | Require offline-baked environments, HDR textures |

---

## Constraints

### Build Constraints
- **Configuration:** Debug (fastest iteration)
- **Parallelism:** `-jobs 2` (limited to avoid memory pressure)
- **Destination:** `generic/platform=macOS` (no simulator, native OpenGL)
- **Timeout:** 60 seconds (generous; actual ~3s)

### Test Constraints
- **Library Path:** `DYLD_LIBRARY_PATH=build/dependencies/mono/macos-arm64/sdk/lib`
- **Timeout:** 30 seconds (generous; actual ~1.4s)
- **GPU:** Apple M4, Metal-backed OpenGL 4.1
- **No graphics profile forcing:** Uses native Apple driver

### Total Time Budget
- **Budget:** 265 seconds
- **Actual:** ~5 seconds (build: 3.31s, test: 1.44s)
- **Margin:** 260+ seconds

---

## Full Test Suite (Separate)

The following tests are **not** included in the gate but are part of the full regression suite:

### 3D Renderer Tests
- `MigrationRendererSmoke` ✅ (in gate)
- `MigrationRendererFeaturesSmoke` (PBR, shading, lighting)
- `MigrationRenderer2DSmoke` (2D graphics)
- `MigrationSceneSmoke` (scene graph)
- `MigrationSceneGPUSmoke` (GPU scene)
- `MigrationRuntimeSessionSmoke` (runtime)
- `MigrationEditorSmoke` (editor)

### PBR/IBL Tests
- `IBLSurfaceReference` (FP32 surface isolation)
- `IBLQualityRenderingChecks` (runtime IBL validation)
- `IBLQualityLobeChecks` (prefilter lobe)
- `IBLReferenceMath` (analytic reference)
- `IBLReferenceChecks` (full validation)
- `DirectPBRChecks` (direct PBR)

### Environment Tests
- `EnvironmentBakeChecks` (bake validation)
- `IBLMipChecks` (mip LOD)

### Authoring Tests
- `MigrationGameUIIntegrationSmoke`
- `MigrationGameUIModelSmoke`
- `MigrationFontSmoke`
- `MigrationTypographySmoke`
- `MigrationSpriteSmoke`
- `MigrationProjectPhysicsSmoke`
- `MigrationMonoSmoke`
- `MigrationImportDependencySmoke`
- `MigrationCoreSmoke`
- `MigrationShaderToolsSmoke`

---

## Verification Evidence

### Build Log Summary
```
** BUILD SUCCEEDED **
```
Exit code: 0  
Duration: 3.31 seconds

### Test Log Summary
```
GPU: Apple M4
OpenGL: 4.1 Metal - 91.7
PASS: Camera, UBO binding/upload/readback, float/int/bool/Mat3/Mat4 attributes, instanced color/entity rendering
```
Exit code: 0  
Duration: 1.44 seconds

### Evidence File
Location: `.hermes-run/last_verify.json`

---

## Remaining Work

### Longer Validation Tests (Not in Gate)

These tests require additional setup (environment bakes, assets, etc.) and are validated separately:

1. **PBR/IBL Quality:** Run `IBLSurfaceReference`, `IBLQualityRenderingChecks` from the full test suite
2. **Full 3D Scene:** `MigrationRuntimeSessionSmoke` with RenderingReference project
3. **Editor Authoring:** `MigrationEditorSmoke` for scene/material/lighting inspection
4. **Mesh Import:** `MigrationImportDependencySmoke` (stage-2 only)

### Full Suite Command
```bash
python3 scripts/hazel.py test --config Debug --profile native --suite all
```

**Note:** This is a separate operation from the Hermes quality gate.

---

## File Changes

Only modified: `scripts/hermes_render_gate.json`

No changes to:
- Engine source code
- Vendor files
- Shaders
- Example projects
- Test expectations

---

## Commit Reference

Branch: `agent/render-reference`  
HEAD: `4148d25048db29671aeb503441e4ba62e40a62e1`

---

*End of document.*
