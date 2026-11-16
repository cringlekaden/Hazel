---
name: hazel-gpu-review
description: Audit or plan Hazel Metal 4/OpenGL rendering, shader portability, PBR/IBL correctness, pass ownership, GPU synchronization, texture formats, or performance. Prefer source-referenced analysis before code changes.
---

1. Establish exact checkout/commit and current implementation. Read only the relevant renderer/shader/presentation code plus the linked `docs/design/` record. If read-only requested, do not modify sources.
2. Trace data ownership end-to-end: scene submission → shared render ordering → backend → GPU resource/pass → presentation/picking/readback. Flag any duplicate rendering path or silent fallback.
3. Check explicit contracts: buffer versions and in-flight retirement; Metal 4 barriers/residency/argument tables; format, sRGB/HDR, clip/depth, UV/orientation, normal/tangent, handedness, integer ID and AA-off silhouette.
4. Distinguish mathematical approximation, visually observed defect, unverified hypothesis and measured correctness violation. Never trade frozen copper error for a dielectric regression or call a diagnostic crop a production pass.
5. Identify the **smallest** decision or experiment that separates competing causes. For performance, request matched work and CPU/GPU timings instead of assuming Metal/compute wins.
6. Report: verified sources, Codex/hardware reports, risk ranking, smallest recommended next slice, gate to prove it, explicit out-of-scope areas. No blanket redesign without demonstrated need.

Helpful references on demand: `docs/agents/ARCHITECTURE.md`, `docs/agents/PROJECT.md`, `docs/design/metal-m1.md`, `docs/design/ibl-view-dependent-investigation.md`.
