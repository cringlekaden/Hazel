# TAA owner visual review

Status: **NOT REVIEWED**. Record owner acceptance separately from automated gates.
Use `feature/3d-foundations` after building the desired configuration and shaders.

```sh
python3 scripts/hazel.py run Hazelnut --config Release --metal --no-restore --project examples/RenderingReference/RenderingReference-XeGTAO.hproj
```

1. Open Scene Settings, select scene-referred presentation, enable HDR TAA and
   Apply. Start with AO disabled. Orbit, translate and dolly slowly, then rapidly;
   stop abruptly. Inspect thin slabs, silhouettes and dark-metal reflections.
   Watch for doubled edges, shimmer, lost detail and delayed highlight response.
2. Move the thin slab independently across the ground in Edit/Simulate. Newly
   exposed ground must recover immediately without persistent dark trails or
   bright leaks. Compare TAA off/on from the same camera. Check selection,
   outlines and gizmos remain aligned; click signed picking targets at edges.
3. Repeat with AO Low, Medium, High and Ultra. Prefer Low/Medium if they retain
   convincing contacts in motion. Use AO visibility/raw/edge inspections to
   distinguish missing AO geometry from temporal filtering. Those inspections
   intentionally bypass temporal history and use fixed spatial sampling.
4. Use the TAA viewport popup: motion, jitter, accepted/rejected history and
   contribution. Red rejection at new surfaces is expected; large static mesh
   interiors should accept history. Debug colors never enter history. Raw HDR
   inspection clamps for display; the internal resolve remains pre-exposure HDR.
5. Compare fine texture/masked edges in the Direct/PBR surface examples and the
   photographic/tiny-emitter IBL studies. Check low-contrast boundaries and
   specular highlights at several roughnesses. Do not approve quality from the
   contact study's mostly flat surfaces alone.
6. Switch perspective/orthographic views, scenes and projects; cut/restore the
   camera; resize repeatedly, minimize/restore, and repeat editor/player launches
   at native 1x and 2x scaling. Open two views on one monitor and move their
   cameras independently. Screen UI/text must remain stationary and exposure
   independent. World 2D preserves its unjittered blending and depth behavior.
7. Save a private copy with TAA enabled and run it in Nutella using `--metal`.
   Repeat from a source-free exported bundle. OpenGL reports TAA unsupported and
   retains its ordinary output. Restore any authored test changes afterward.

Record configuration, extent/scale, scene/camera path, AO preset, issue timestamps
and native moving captures. Approve sharpness, responsiveness, ghosting and AO
stability explicitly, or list remaining defects. Automated pixel checks and GPU
validation do not substitute for this review.
