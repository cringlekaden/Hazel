# Add C# behavior

Hazel scripts derive from `Hazel.Entity`. Scene entities own their authored fields; a project supplies its own assembly. Hazelnut and Nutella use the same managed API and RuntimeSession lifecycle.

## Create, compile and assign

1. Open a project in Hazelnut and select **Project > Create Script…**. Choose a namespace/class and open the source in your configured editor.
2. Add behavior, then use **Project > Build Scripts…**. Compilation needs the prepared [source SDK and platform compiler prerequisites](getting-started.md#prerequisites-installed-by-you).
3. Select an entity, add **Script**, choose its class and assign supported public fields in Properties. Entity, prefab, sprite-region and clip fields use typed project/scene selectors.
4. Save and Play. **Project > Reload Scripts** reloads changed assemblies; Stop restores authored defaults.

A minimal script (replace the namespace with your project's namespace):

```csharp
using Hazel;

namespace MyGame {
    public class Drifter : Entity {
        public float Speed = 2;
        void OnUpdate(float dt) {
            var position = LocalTranslation;
            position.X += Speed * dt;
            LocalTranslation = position;
        }
    }
}
```

From the checkout, compile an example independently of native builds:

```sh
python3 scripts/hazel.py script-build examples/Sandbox/Sandbox.hproj --config Debug
python3 scripts/hazel.py run Hazelnut --config Debug --project examples/Sandbox/Sandbox.hproj
```

Use `python` on Windows. `run` launches existing products. Project source/Premake definitions normally live in `Assets/Scripts/`; modules deploy to the descriptor's `ScriptModulePath` (examples use `Scripts/Binaries/<Project>.dll`). Debug/Release compilation workspaces stay in `build/`. Do not copy arbitrary assemblies into application Resources to replace project compilation.

## Lifecycle and identity

Use `OnCreate()`, `OnUpdate(float)` and `OnDestroy()` for runtime behavior. Cache component wrappers used repeatedly. `GetComponent<T>()` returns null when absent. Entity wrappers belong to a scene lifetime: check `IsValid`, reacquire after transitions and compare `.ID` within the same live scene rather than managed object-reference equality.

`Entity.Instantiate(prefab, position)` creates a detached subtree with remapped internal references. Its physics is usable immediately; managed startup runs at the safe lifecycle boundary and `As<T>()` is available after startup. `Destroy()` invalidates immediately and defers cleanup outside callback iteration. `LocalTranslation/Rotation/Scale` are local to the hierarchy; rotation is radians. Legacy `Translation` is world-space. Parenting requests are queued and report their result after a boundary.

`Scene.LoadScene("Scenes/Level1.hazel")` requests a project-owned scene transition. The first accepted request wins until the next session boundary. Stop cancels pending work. See [SceneTransitions](../examples/SceneTransitions/README.md) and [authoring contracts](authoring-reliability.md).

Loading/validation failure leaves the current runtime intact. A native failure after target startup begins retires that partial target and restarts the previous scene, preserving its scene/session identity but recreating scripts, physics, audio and UI state. Its component values remain on that scene; live managed fields restart from authored values. Reacquire script instances.

Recovery discards failed-target exit/transition requests and retired input. Accepted storage/gain changes and arbitrary script side effects are not rolled back. If restarting the previous scene also fails, the session stops and propagates the error. The host can report `RuntimeSession::GetError()`; recovery is not a transaction over game code.

## Connect UI, storage and audio

Author UI geometry/styles in Hazelnut. Assign a screen/control's **Event owner** to the controller entity; in its callback drain `GameUI.TryConsumeEvent`. Buttons on ordinary canvases can also use the documented unowned action-polling form. [Runtime UI](runtime-game-ui.md#connect-c-decisions) covers the distinction, silent setters and queued screen/tab requests; [UITutorial.cs](../examples/Sandbox/Assets/Scripts/Source/UITutorial.cs) is a complete example.

[SaveData and audio](runtime-storage-audio.md) documents slot/payload validation, persistence boundaries and `AudioSourceComponent`. Player preferences and saves do not rewrite the project or editor profile.

Prepared games ship their runtime and compiled modules. Players need no compiler, Python or source SDK. Changed scripts and export require the [build toolchain](getting-started.md); [packaging](packaging.md) validates managed dependencies before distribution.
