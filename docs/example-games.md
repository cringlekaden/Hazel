# Example games milestone

Historical example-game milestone record. Current prefab, lifecycle, level and editor workflows are recorded in [editor-authoring.md](editor-authoring.md).

Nutella implementation `5d972f3` passed Windows/Linux CI 37100603151; its
documentation tip `dbd840e` was merged without force into master at `7a0eec2`.
Work continues on feature/example-games, without automatic merge. The saved
stash, editor layout and ignored VS Code settings remain preserved.

Audit: RuntimeSession owns Play/player lifecycle and deferred transitions; scenes
copy authored fields; projects own asset roots and script environments; Scene
creates physics before managed OnCreate. Existing component bindings expose text,
translation, impulse and input. Stable content can be authored directly in .hazel.
No entity spawning/destruction API is needed: Skybound reuses a fixed authored
obstacle pool; MeadowRun retains five collectibles. No new runtime path is needed.

Required engine corrections: expose writable runtime body velocity for responsive
top-down control, author/serialize body gravity scale (default 1 for existing scenes),
and synchronize scripted translation with a live physics body for checkpoint
teleports. Expose orthographic camera size and read-only viewport aspect to fit a
fixed game area on resize. Bindings follow existing component/internal-call names;
editor controls expose gravity. No game-specific engine code or UI framework.

MeadowRun: a compact original pixel-art garden expedition, five lantern seeds,
solid trees/rocks, a pond checkpoint and a gated trail exit. Separate title, meadow
and completion scenes. Skybound: original wind-sprite flight, ready/flying/dead
states, fixed-step motion/collision, four recycled obstacle pairs, once-only score,
edge-triggered Space/click and restart/menu. Scene structure is authored; scripts
handle motion/rules/buttons. Useful numeric tuning is serialized.

Camera policy: fit a 16 x 12 world area; extra aspect space reveals decorative
background, never changes gameplay bounds or speed. Mouse mapping uses the same
camera and RuntimeSession viewport coordinates in both applications. Original
small RGBA pixel assets are checked in; one optional Pillow generator retains
source/provenance. No runtime art dependency or audio subsystem.

Canonical SDK commands build scripts, run/edit projects and package complete
Nutella games. Existing SceneTransitions distribution/regressions remain intact.
CI preserves normal application artifacts and separately publishes named game
archives after actual extracted acceptance. Tests cover physics/camera bindings,
fixed-step gameplay invariants, transitions, editor Stop isolation, project switch,
real mouse input, resize and source/SDK-free relocation. Screenshots are inspected;
automated desktop input is distinct from physical human playtesting.

## Verification

API checkpoint `f63c96a` passed all 13 native Linux Debug regressions and
[Windows/Linux CI 37114234601](https://github.com/cringlekaden/Hazel/actions/runs/37114234601).
MeadowRun is `d2309bb`; Skybound is `7c21c45`. Native HD4000 desktop tests finish
MeadowRun with real WASD input and exercise completion/restart/menu in player and
editor Play. Skybound's actual controls score, die, restart and return in both.
The runtime regression uses ScriptEngine::Init's existing project transaction,
keeping Mono's root alive while replacing project domains; it verifies class
isolation across MeadowRun -> Skybound -> MeadowRun. Tests preserve authored
fields/scenes, repeat completion/checkpoint transitions, and resize to portrait.
Flight invariants pass 20 simulated minutes with four original gate objects and
610 once-only points, plus 30/120 fps equivalence and ready/death input isolation.

Initial rendered inspection corrected star scale, title contrast, column caps,
ground tiling and text centering. Captures use actual OS windows or the engine's
framebuffer. The actor-control test follows the rendered coat in a bounded search
area, allowing texture sampling differences at smaller editor viewport sizes.
Software drivers are copied only into private test directories. Complete named
archives use the canonical --name option; README/license files travel with projects.
Final source Release desktop acceptance and both named native Linux extracted
packages pass at `7e8d844`. Checkout resources and the Mono SDK are hidden during
package execution; archive/file checksums and clean commit metadata are audited.
Saved imgui.ini and local VS Code settings match their preserved backups. Vendor
submodules remain pristine. [Final CI 37118805605](https://github.com/cringlekaden/Hazel/actions/runs/37118805605)
passes on both platforms: Debug/Release regressions and game authoring/play,
normal application archives, and extracted games with software and OpenGL 4.1
profiles. Named archives are published separately from testing evidence. Downloaded
Windows/Linux game and application archives pass SHA-256,
all-file manifests, clean commit metadata and absence-of-test-driver audits.
Downloaded Ubuntu CI game archives additionally pass native Intel HD4000 gameplay
and source/SDK-free relocation on Grandpa. Windows packaged captures are also
visually inspected. Windows graphics checks use
isolated software graphics, not physical GPU hardware. Physical human playtesting
and additional Windows hardware/high-DPI combinations remain manual checks.

The last authoring pass enlarges essential HUD labels for the editor viewport and
reads Skybound initial obstacle positions/gaps and button positions from authored
entities. Windows desktop evidence identified a test synchronization race: a
transition log precedes a displayed frame. Tests now observe the actual rendered
controls before subsequent clicks and retain failure captures. Production scene
transition/lifecycle behavior is unchanged.
