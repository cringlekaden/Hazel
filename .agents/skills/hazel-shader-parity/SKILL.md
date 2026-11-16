---
name: hazel-shader-parity
description: Qualify Hazel GLSL/SPIR-V/MSL shader porting and Metal 4/OpenGL rendering parity, including bindings, depth/IDs, numerical lighting and packaging. Use for shader/backend translation work.
---

Read task-specific `docs/design/` gates, current shader compiler/variant pipeline and relevant `docs/agents/QUALITY.md`. Do not resume deferred IBL research merely because parity exposes existing split-sum limits.

1. Establish canonical input shader, pinned shaderc/SPIR-V/SPIRV-Cross/MSL toolchain, exact stage/variant, compile mode and cache/artifact identity. Generated MSL is output, not a second hand-edited shading model.
2. Verify reflected resource and argument-table mapping: stage-local bindings, vertex streams, CPU offset/size/alignment, array strides, samplers, cube arrays, integer targets, discard/derivatives/LOD, and failure diagnostics.
3. Compare conventions explicitly: OpenGL vs Metal clip depth, render-target origin, projection reconstruction, UV and `dFdy` signs, winding, normal/tangent handedness, sRGB transfer, HDR FP32, depth/stencil, integer picking clear `-1`, AA-off silhouettes and sampling seams.
4. Use minimum discriminating fixtures: 2D quad/text/ID, perspective+ortho geometry, unlit+direct+IBL 3D, selection, roughness/metal sweeps and authored versus runtime. Preserve frozen references and held-out lighting; do not broadly weaken tolerances.
5. Validate source-free artifact packaging/relocation only when packaging is in scope. Report first bad stage (compile, reflection, pipeline, draw, target, readback, visual, numerical, export), measured deltas and native GPU evidence.
6. Run narrow checks during iteration, milestone suite at authorization boundary; output PASS/FAIL/NOT RUN/BLOCKED by backend/platform. No silent Metal->OpenGL shader fallback.
