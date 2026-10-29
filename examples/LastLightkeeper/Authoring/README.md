# Historical Lightkeeper authoring evidence

**Retained 2026-10-28; pre-runtime-UI composition.** Use [Hazelnut](../../../docs/editor-guide.md) for current content and the [game guide](../README.md) to play/export. These files explain how earlier scenes were assembled; they do not reproduce later UI, style, screen or property/history work.

`compose.py`, `cliff_chapter.py`, `observatory.py` and `tower_chapter.py` are one-time generators. They require PyYAML/Pillow only for historical reproduction, never to run/build/export the game. Writing requires both `--historical-rebuild --write`; `--overwrite` additionally permits replacement of existing scenes. They **refuse a maintained checkout** (including Git worktrees). Some historical outputs include prefabs/reference images in addition to scenes.

If investigating old output, copy this complete project outside the checkout into disposable storage, inspect each script and use that copy only. Do not invoke the generators against accepted project content or treat regenerated output as an upgrade. Normal changes preserve region IDs and use native editor services.

The [history index](../../../docs/history.md) lists revision notes, chapter checkpoints, validation and capture records. Original asset licenses and provenance remain in [Assets/Licenses](../Assets/Licenses/PROVENANCE.json); real captures and reference collages are distinguished in [Captures/README](Captures/README.md).
