# The Last Lightkeeper — feasibility and content plan

Baseline: current master `463cbdd`, 2026-10-10. Work is isolated on
`feature/last-lightkeeper`; no merge, vendor-pin updates, macOS or Metal work.
Repository inspected: Premake/native and managed builds, Nutella RuntimeSession,
Hazelnut project/scene/prefab/sheet authoring, Mono bindings, ProjectAssets,
physics, renderer, filesystem, dependency closure, CLI, existing examples,
native/service/desktop regressions and Linux/Windows CI.

## Capability map and bounded gaps

| Requirement | Verified implementation / use |
| --- | --- |
| Authored locations | Version 2 `.hazel` scenes, Hazelnut hierarchy/local Inspector; scenes saved as ordinary assets, not runtime tile generation. |
| Prefabs | Detached subtree instantiation, UUID/reference remapping, destruction at safe boundaries (`Prefab`, `Entity.Instantiate`). Reusable scene decorations and light-beam segment prefab. |
| Scene changes | `Scene.LoadScene` queues one asset-relative transition; RuntimeSession validates before retiring old scene. Menu → island → endpoint. |
| Hierarchy | Root rigidbodies; visual/HUD children use local transforms. Camera children form screen UI. Keep-world reparent rejects shear. |
| Animation | `.hsprites` regions/clips, nearest sampling, SpriteAnimationComponent. Kenney Tiny characters are static poses, not walking sheets: no invented animation coverage. Actual light clips and movement bob are suitable for the slice. Directional walking art remains a specific later art task. |
| Movement / collision | Box2D root dynamic body, zero gravity, writable velocity, fixed rotation; static authored colliders. Hazard proximity is ordinary game logic, since managed collision callbacks/raycast are absent. |
| Mirror puzzle | Discrete authored grid and reflect/occlusion solver in game C#. Renderer2D can draw textured beams; no lighting engine or general puzzle framework needed. |
| HUD / dialogue | TextComponent plus asset panels/icons as camera children. No stock ImGui in player. C# interaction proximity, edge input, queued authored dialogue; no general dialogue engine needed. |
| Mouse / keyboard | Runtime viewport world projection and WASD/arrows/E/Space/Escape/Enter. Controls use edges, normalize diagonals, ignore dialog movement. |
| Lifecycle | Scene copy isolates authored data, physics before OnCreate, explicit OnDestroy, managed domain retained on scene change and replaced on project change. Reset game static progress on menu load from session storage. |
| Assets / export | Project-relative owned assets, image/sheet cache, stable IDs, CPU sheet audit, managed closure, Mono/native/runtime licenses, relocated Nutella discovery. Pack notices inside Assets guarantee inclusion. |
| Durable progress (gap) | Add bounded native runtime storage, descriptor-owned save namespace, versioned envelope and atomic writes. Game payload has its own schema. Editor sessions default to memory-only; Nutella opts into persistence. No writes to authored scenes or installation. |
| Audio (gap) | Add bounded miniaudio playback, pinned source and license, authored source component and managed controls; scene/session cleanup. Device failure permits silent play with full visual cues. No mixer editor, streaming system, music synthesis or spatial engine. |

No other engine system is justified. The GL 4.1/X11 and Windows 10 requirements
remain. CPU asset audit/package smoke cannot establish physical GPU/audio quality.
Linux can be exercised locally; Windows requires CI/hardware evidence. Native audio
must not introduce unbundled non-system libraries. Save corruption/future versions
must produce visible recovery feedback, never silently erase a player's file.

## Asset review and visual direction

Reviewed Kenney's free catalog and Tiny Town, Tiny Dungeon, Tiny Factory, Tiny
Battle, Roguelike/RPG and UI Pack - Pixel Adventure pages, then actual downloaded
sheets. Choose the matching Tiny family, rather than mixing the older RPG palette.
Use muted blue-gray stone and storm-tinted green/sand, with warm amber machinery
and restored light. Tiny Battle contributes only shoreline/water, not military art.
UI uses the Small tiles / Thin outline subset of Pixel Adventure, matched to the
same pixel scale. OpenSans is Hazel's existing licensed font for readable prose.

| Imported pack | Coverage / size |
| --- | --- |
| Tiny Town 1.1 | 16×16 grass, sand paths, trees, cottage, tower stone, tools; packed 192×176. |
| Tiny Dungeon 1.0 | 16×16 player/NPC poses, stone, lens, torch, small enemy/effects; packed 192×176. |
| Tiny Factory 1.0 | 16×16 pumps, pipes, switches, gears, warning markings; packed 192×176. |
| Tiny Battle 1.0 | 16×16 rounded shoreline and water; packed 304×176. |
| UI Pack - Pixel Adventure (included license says 2.0) | Selected 16×16 thin-outline icons/panels; page update says 1.0, recorded discrepancy rather than overriding included license. |
| Interface Sounds 1.0 | Selected original Ogg UI/repair/error cues; PCM conversion for bounded runtime decoder. |

All six included License.txt files were read and state CC0. Exact source URLs and
archive SHA-256 are in Assets/Licenses/PROVENANCE.json. No logo imported. Credits
name Kenney and explicitly do not imply endorsement. Nearest min/mag, no mipmaps,
clamp for region sheets. One world unit is one 16px tile; actor at comparable scale.
Import full packed sheets into native `.hsprites`; keep IDs stable and use
Hazelnut's sheet editor for subsequent slicing/clip changes. Missing distinctive
mirror face/lighthouse optics can be a small original compatible asset, documented
separately; never replace the environment with generated placeholders.
A visual reference/contact sheet and island plan will accompany authored assets.

## Authored play and progression

Premise: keeper Iona returns after a storm. Ferryman Orrin waits for a signal;
mechanic Mara left the pump instructions. Restore the lower signal so rescuers can
reach the island. Restoration visibly changes machines and light, and each step
reveals a new route or understanding.

Slice target **8–12 minutes**, subject to hands-on timing: menu/continue; Harbor
Landing tutorial and keeper's journal; one mirror opens a route; Salt Grove's
optional story/cache; Pump Works repair with recovered wrench; telegraphed tide
lane with safe alcoves and recoverable checkpoint; lens recovery; three-mirror
relay with visible traced beam, obstruction and receiver; lighthouse ignition,
a short authored resolution and a view toward the next chapter. No repeated room
chain, grind, arbitrary wait timer or full-game claim. Escape opens pause/menu;
WASD/arrows move, E interacts/rotates/talks, Enter/Space advances dialogue, Q opens
journal. Hazard resets position to the most recent safe checkpoint and retains
restoration. Save on meaningful progression; visible save-failure feedback.

Scene map: MainMenu → Breakwater (connected harbor/grove/pump/relay/tower districts)
→ Dawn (slice ending/credits), with explicit return/continue routes. Entire island
is ordinary editable scene content. Camera follows within bounds, HUD stays
anchored; title/ending use composed coastal vignettes from the same sheets.

Later milestones, total target **30–45 minutes**:
1. Lower signal slice, test readability and measure actual pacing.
2. Cliff cistern/sea caves: lantern upgrade, movable shutter puzzles, one distinct
   enemy encounter, Mara's reunion; ~8–10 minutes.
3. Abandoned observatory: paired beams and lens upgrade, character choice/payoff;
   ~8–10 minutes.
4. Lighthouse ascent: storm-heart final encounter combining established mechanics,
   restoration panorama and rescue ending; ~6–10 minutes.
Each location must change composition, puzzle grammar and story; no copied rooms.

## Checkpoints, risks and acceptance

Commit audit/assets first; reusable storage/audio separately; authored slice and
mechanics; then polish/validation. Builds serialized, two compiler jobs, Debug and
Release. Relevant existing regressions plus focused save/corrupt/version/isolation,
reflection/obstruction/progression/hazard tests, native asset closure, script build,
relocated package startup. No coordinate-guessing full-game bot.

Acceptance: clean schema/closure; editable scenes/sheets/prefabs; title→play→ending
and continue; complete causally ordered restoration; responsive diagonal-normalized
motion and solid scenery; legible context prompts and feedback; hazards offer
advance warning and safe recovery; game survives restart/relocation; notices ship;
rendered frames inspected where display works. Physical playtime, art quality and
Windows audio cannot be inferred from unit tests. Log all unverified acceptance
items and stop if any fundamental blocker demands an architectural hack.
