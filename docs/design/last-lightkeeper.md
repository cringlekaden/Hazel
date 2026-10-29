# The Last Lightkeeper — feasibility and content plan

> **Historical scope — marked 2026-10-28.** This record describes its named checkpoints, including features absent or pending then. It is not current setup, support status or a new feature roadmap. Use the [current guides](../README.md) and [history index](../history.md).

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
and restored light. Tiny Battle contributes shoreline/water and energy feedback; military props are excluded.
UI uses the Small tiles / Thin outline subset of Pixel Adventure, matched to the
same pixel scale. OpenSans is Hazel's existing licensed font for readable prose.

| Imported pack | Coverage / size |
| --- | --- |
| Tiny Town 1.1 | 16×16 grass, sand paths; 16×32 tall pines; cottage, tower stone, tools; packed 192×176. |
| Tiny Dungeon 1.0 | 16×16 player/NPC poses, stone, lens, torch, small enemy/effects; packed 192×176. |
| Tiny Factory 1.0 | 16×16 pumps, pipes, switches, gears, warning markings; packed 192×176. |
| Tiny Battle 1.0 | 16×16 rounded shoreline and water; packed 288×176. |
| UI Pack - Pixel Adventure (included license says 2.0) | Selected 16×16 thin-outline icons/panels; page update says 1.0, recorded discrepancy rather than overriding included license. |
| Interface Sounds 1.0 | Selected original Ogg UI/repair/error cues; PCM conversion for bounded runtime decoder. |

All six included License.txt files were read and state CC0. Exact source URLs and
archive SHA-256 are in Assets/Licenses/PROVENANCE.json. No logo imported. Credits
name Kenney and explicitly do not imply endorsement. Nearest min/mag, no mipmaps,
clamp for region sheets. One world unit is one 16px tile; actor at comparable scale.
Import full packed sheets into native `.hsprites`; keep IDs stable and use
Hazelnut's sheet editor for subsequent slicing/clip changes. The revision uses existing Tiny Dungeon mirror art and native 8-pixel crops
for shore/gravel adjacency. No new original artwork was necessary; the ordinary
worker and wisp poses match the environment palette.
A visual reference/contact sheet and island plan will accompany authored assets.

## Authored play and progression

Premise: keeper Iona returns after a storm. Ferryman Orrin waits for a signal;
mechanic Mara left the pump instructions. Restore the lower signal so rescuers can
reach the island. Restoration visibly changes machines and light, and each step
reveals a new route or understanding.

The author completed the original lower-signal slice in **about five minutes**
after learning its mechanics. This is the actual pacing evidence. The revised
Breakwater and new cliff chapter are not yet human-timed. No automated traversal
or mechanics test establishes playtime or artistic quality.

Breakwater: harbor reflection/bridge; grove tools and notes; pump repair and tide
crossing; lens; service receiver/latching door; third mirror room; stair receiver;
lower beacon. Optional repair book and sea glass reward discovery. Iona notices
repair evidence, Orrin needs a navigable reef to reach stranded people, and Mara
records practical repairs while challenging unsafe keeper habits.

Cliff Cistern: workshop sleeve upgrades the portable lantern. Hold Space to focus
along the last movement direction; movement slows while focused. Physical shutter
slats leave one indexed opening. A drain receiver opens the raised inspection walk;
the raised slot and fixed mirror route release the cliff bridge. Receivers latch,
so route changes do not undo repairs. The cistern's larger pool and masonry walks
contrast with Breakwater's grove and landing.

Sea Caves: exposed rock and old relay masonry; upper gallery receiver through the
first shutter; fixed mirror route through a second shutter into Mara's inspection
window. The wisp fixes a rush target before lunging: move off the visible mark or
focus toward it within range. A hit returns to a dry checkpoint and retains repairs.
The eastern checkpoint activates when reached. Light opens Mara's door; meeting
her grants the winch key, opens a dry lower passage and starts her authored return
route. The landing resolves this rescue and reveals people still at the observatory.
No arbitrary waiting, fetch repetition or extra-distance playtime claim.

Scene map: MainMenu → Breakwater → CliffCistern → SeaCaves → CliffDawn →
Observatory → ObservatoryDawn → UpperTower → HarborHome → Ending/Credits →
MainMenu. Completed Continue revisits HarborHome. Cliff Dawn remains the departure
screen for old completed cave saves. The observatory's extended payload is LK3;
it retains LK1/LK2 data and adds carriage position and a separate terrace marker.
Continue uses completed milestones plus the cave entry/recovery marker. LK1 saves
keep all existing data; their completed court gets a compatible service-door default.
LK2 stores shutter settings and a separate cave marker under the same native save
ownership/envelope. Corrupt/future records remain intact. Editor Play remains
memory-only, including direct chapter-scene practice. LK4 retains LK3 and adds
the tower checkpoint and three-contact bitmask; completed contacts, drive power,
starting the main light and boat departure are separate, failure-safe milestones.

All locations, physics boundaries, movable slats, optical windows, fixed mirrors,
receivers, character poses, HUD, cue sources and transitions are ordinary authored
assets. Physics owners remain roots. Only a bounded pool of native light prefabs
is instantiated at runtime. Dialogue, optics, AI, interaction and progression stay
in game C#; no additional gameplay engine system was needed.

The eventual target remains **30–45 minutes**, pending complete authored content
and timed human play. The gameplay route now includes lower beacon, cliff rescue,
observatory and upper tower. The observatory adds opposed long-focus sightlines,
a winched survey carriage and Nessa/Cal's calibration. The tower combines the
fixed entrance mirror, exposed approach, three independent sighting contacts,
stone cover and the learned telegraphed wisp. Repairs persist through recovery.
Starting the main light returns to the landing; short character conversations
and a boat departure resolve the rescue, followed by ending and credits.
Next: reusable runtime UI, Hazelnut authoring and presentation migration.
No completed-game duration is claimed before hands-on timing.

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
