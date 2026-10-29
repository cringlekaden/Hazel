# Historical records

Current instructions live in the [documentation index](README.md). Records below preserve checkpoint facts, unsuccessful approaches, old commands and measured evidence. Their “current”, “pending”, test counts and branch names are relative to the revision recorded in each document.

The active [post-UI cleanup audit](design/post-ui-cleanup-audit.md) keeps its original `650d09e` baseline and implementation appendices. It is a maintainer work record, not the public getting-started guide. Its remaining evidence-backed items precede final qualification/acceptance and integration; it does not restart the UI feature roadmap.

## Record inventory

| Record | Disposition |
| --- | --- |
| [docs/editor-authoring.md](editor-authoring.md) | Historical design / qualification: Editor authoring milestone |
| [docs/nutella-runtime.md](nutella-runtime.md) | Historical design / qualification: Nutella milestone |
| [docs/example-games.md](example-games.md) | Historical design / qualification: Example games milestone |
| [docs/design/editor-usability-a1-a2.md](design/editor-usability-a1-a2.md) | Historical design / qualification: Editor usability implementation: A–H |
| [docs/design/editor-usability-audit.md](design/editor-usability-audit.md) | Historical design / qualification: Hazelnut usability and authoring audit |
| [docs/design/last-lightkeeper-completion.md](design/last-lightkeeper-completion.md) | Historical design / qualification: Completion checkpoints and runtime UI design |
| [docs/design/last-lightkeeper.md](design/last-lightkeeper.md) | Historical design / qualification: The Last Lightkeeper — feasibility and content plan |
| [docs/design/runtime-ui-integration.md](design/runtime-ui-integration.md) | Historical design / qualification: Runtime UI integration |
| [docs/design/runtime-ui-model-checkpoint.md](design/runtime-ui-model-checkpoint.md) | Historical design / qualification: Runtime UI model checkpoint |
| [docs/design/runtime-ui-production-audit.md](design/runtime-ui-production-audit.md) | Historical design / qualification: Runtime UI production audit and implementation design |
| [docs/design/sprite-sheet-authoring.md](design/sprite-sheet-authoring.md) | Historical design / qualification: Sprite sheets and frame animation in Hazelnut |
| [docs/migration/KNOWN-LIMITATIONS.md](migration/KNOWN-LIMITATIONS.md) | Historical design / qualification: Known limitations and portability boundaries |
| [docs/migration/PRESERVATION.md](migration/PRESERVATION.md) | Maintainer invariants / import evidence: Preservation decisions |
| [docs/migration/PROGRESS.md](migration/PROGRESS.md) | Historical design / qualification: Migration progress and final parity |
| [docs/migration/PROVENANCE.md](migration/PROVENANCE.md) | Maintainer invariants / import evidence: Source and dependency provenance |
| [examples/LastLightkeeper/Authoring/Captures/README.md](../examples/LastLightkeeper/Authoring/Captures/README.md) | Historical design / qualification: Actual captures |
| [examples/LastLightkeeper/Authoring/OBSERVATORY-CHECKPOINT.md](../examples/LastLightkeeper/Authoring/OBSERVATORY-CHECKPOINT.md) | Historical design / qualification: Observatory playable checkpoint |
| [examples/LastLightkeeper/Authoring/REVISION-NOTES.md](../examples/LastLightkeeper/Authoring/REVISION-NOTES.md) | Historical design / qualification: Chapter revision and continuation |
| [examples/LastLightkeeper/Authoring/TOWER-CHECKPOINT.md](../examples/LastLightkeeper/Authoring/TOWER-CHECKPOINT.md) | Historical design / qualification: Upper tower and full-game endpoint |
| [examples/LastLightkeeper/Authoring/VALIDATION-CHAPTER-ONE.md](../examples/LastLightkeeper/Authoring/VALIDATION-CHAPTER-ONE.md) | Historical design / qualification: Validation record |
| [examples/LastLightkeeper/Authoring/VALIDATION.md](../examples/LastLightkeeper/Authoring/VALIDATION.md) | Historical design / qualification: Revision validation |

Migration `evidence/` retains original import manifests, comparison TSV/JSON and font-pin records. They are historical source/provenance, rather than generated build output to clean. Original Lightkeeper reference collages and captures retain their original captions/scope. Current public screenshots live in [images](images/README.md).

## Historical tools

[Lightkeeper Authoring](../examples/LastLightkeeper/Authoring/README.md) records the pre-runtime-UI composition generators. They are not current authoring recipes and refuse writes in the maintained checkout. Never regenerate accepted example scenes to validate documentation.

The [upstream comparison utility](../scripts/migration/compare-upstream.py) compares historical local baseline `b030be7` against public upstream `1feb70572fa87fa1c4ba784a2cfeada5b4a500db`. It now requires a **new scratch output directory** rather than defaulting to tracked evidence. Example (replace the upstream path, and choose a destination that does not exist):

```sh
python3 scripts/migration/compare-upstream.py /path/to/upstream-checkout --output /tmp/Hazel-import-comparison-new
```

The old `docs/migration/evidence` default in historical prose describes the original procedure; do not overwrite that evidence. [Native diagnostics](development.md#native-tools), including `measure-scene`, have their maintained home in the development guide.
