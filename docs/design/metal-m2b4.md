# Metal 4 M2-B4: bounded production framebuffer integration

Baseline `06cc2adb9d3b9fcf23565fe3441d9b163d8eb491` on
`feature/3d-foundations`, synchronized with origin. Four unrelated untracked
.DS_Store files are preserved. This checkpoint connects the existing Framebuffer
factory to Metal's shared Renderer2D quad submission; no presentation, compositing,
outline, blending, additional formats or scene/editor integration is authorized.

Predeclared gates:
- Framebuffer::Create returns a production Metal resource for exactly ordered
  RGBA32F / signed R32Sint / Depth32F, single sample, offscreen extents 1..32.
  Other profiles/operations reject explicitly. OpenGL behavior stays unchanged.
- Bind/Clear, Renderer2D scene/ordered flush, Unbind, explicit nonblocking Submit,
  Poll and color/ID/depth readback use the production resource boundary. All load
  actions clear, all stores preserve attachment data; no implicit load/preservation
  across submissions is claimed. No GL framebuffer IDs or ambient GL state emulation.
- Framebuffers own immutable submitted attachment generations; resize, replacement,
  destruction and unsubmitted cancellation while three GPU submissions are gated
  must retain pending generations through M1 completion plus feedback. Ordinary
  submissions/resize never globally wait; busy slots retain a retryable CPU packet.
- Preserve B3 quad packing, texture versions before flush, ordering, camera/UV,
  signed IDs and depth behavior with the current eight-quad qualification bound.
  The bound belongs to the experimental encoder profile, not the Framebuffer API.
- Debug/Release API/shader validation, exact signed IDs/clears, absolute color/depth
  error <=1e-6; reuse all B3 fixtures through the factory, add asymmetric resize
  and lifecycle fixtures, twelve successive submissions and pending-work shutdown.
- Focused native OpenGL regressions, warning/direct-header/SDK checks and scheduled
  public documentation health check; unavailable platforms reported honestly.

Implementation and acceptance evidence follow after qualification.

## Production ownership and pass contract

Framebuffer::Create selects MetalRendererAPI::CreateFramebuffer, which rejects
creation after backend shutdown and constructs MetalFramebuffer on the same M1
device. DEPTH32F is a distinct explicit format; the existing Depth alias remains
DEPTH24STENCIL8. The OpenGL implementation and its existing bind/clear/read APIs
are unchanged. Newly added explicit asynchronous base operations reject on
backends that do not implement them; they do not silently fall back to OpenGL.

MetalFramebuffer owns its specification, native attachment generation, unsubmitted
CPU packet and latest readback association. It is noncopyable; callers share Ref
ownership. No raw backend pointer survives backend release: capture operations
resolve and validate the current backend/device, while polling/completion retain
the device through shared ownership. The backend holds only a weak reference to
the currently bound CPU packet, so destroying its framebuffer cancels unsubmitted
work rather than making an invisible global framebuffer owner.

Construction and resize allocate all three private render-target textures before
publishing the generation/specification. A successful submission marks that
generation used; the next Bind allocates a replacement, including when the prior
GPU work is still pending. Resize replaces the resource generation without
waiting. Submitted MetalQuadFrame packets retain the original generation through
M1 completion/feedback. Framebuffer destruction cannot free those textures early.
This conservative versioning does not yet pool/recycle completed attachments.

Bind starts one explicit offscreen pass and captures the qualified Quad32 shader.
It never installs a GL framebuffer name or ambient GL raster state. Multiple
unbound resource-owned packets can coexist; submitting one does not change the
other's binding/camera/geometry. Nested binding rejects. EndScene precedes Unbind;
active-scene unbind/cancel/clear rejects without altering CPU submission state.
Normal callers end their scene before destroying its framebuffer. Destruction
implicitly discards an unsubmitted packet without touching submitted GPU work.

Each Bind defaults to color zero, signed ID -1 and depth one. Clear(color) captures
finite RGBA32F color and resets the signed clear to -1; ClearAttachment(1, value)
can override the exact signed int32 clear. Clears must precede scenes/draws and
are encoded as native load-action Clear at Submit, once per pass. Every attachment
uses Store. Multiple Renderer2D scenes/flushes inside that pass preserve prior
draws. A new pass clears instead of loading previous submitted pixels; Load,
discard, partial clears and configurable depth clears are not exposed here.

Submit requires an unbound packet. It returns a CPU readback handle on acceptance,
or null while all M1 slots are busy, retaining the identical packet for retry or
Cancel. Poll is nonblocking. ReadColorPixel(0), ReadPixel(1) and ReadDepthPixel
poll and reject pending/missing/error/out-of-bounds data; they never add a GPU
wait. Returned FramebufferReadback handles keep completed CPU arrays independently
of resize/replacement/destruction. Bind/Resize/Cancel clear the resource's readback
association, while previously returned handles remain valid. Poll and readback
publication retain the existing single render-thread ownership contract.

Cancel discards unsubmitted work and its local readback association; it never
aborts a submitted command or frees completion-retained attachments. Unsupported
profiles, GL attachment IDs, encoding/compositing/outlines and integer clears of
other attachments reject explicitly. Ambient RenderCommand clear/clear-color and
viewport calls reject while a framebuffer is bound, directing callers to this
explicit boundary; M1's ordinary surface-clear behavior is preserved otherwise.

MetalDeviceQuad now consumes the framebuffer-owned attachment generation rather
than allocating a second set of targets during submission. The old backend
BeginOffscreen/SubmitOffscreen/CancelOffscreen entry points and native-only
readback type are removed. The public FramebufferReadback record is the sole
production completion payload. Quad packing, texture admission/versioning,
ordered flushes, the 32-slot descriptor ABI, artifact loading and native MTL4
encoding remain the same B3 path. No GLSL or lighting mathematics changes.

M1's queue, three command allocators, shared completion event, feedback, residency,
render-to-copy barrier and shutdown drain are reused. No ordinary operation waits
globally. The eight-quad/1..32-extent restrictions remain the current qualification
profile: Framebuffer specifications/readback APIs have no permanent quad-count
limit, and this checkpoint introduces no generalized render graph or pass system.

## Reproduction

Use the existing B1 producer if shader packages are unavailable, then the focused
runner extended in place (no second test-side renderer or Metal encoder):

```sh
python3 scripts/macos/metal_shaders.py --config Debug --build --output build/metal-quad-resources/shaders/metal
python3 scripts/macos/metal_m2b2.py --config Debug --build --resources build/metal-quad-resources --output build/metal-m2b4-evidence/debug-acceptance
python3 scripts/macos/metal_m2b2.py --config Release --build --resources build/metal-quad-resources --output build/metal-m2b4-evidence/release-acceptance
```

Fresh evidence directories are required. Actual local runs reuse the unchanged
`build/metal-m2b2-evidence/Resources` bundle. Runtime PATH and DEVELOPER_DIR are
unavailable; Metal API and shader validation are enabled. No window is created.

Build only the selected targets through Premake's maintained producer:

```python
import sys
sys.path.insert(0, 'scripts')
import hazel
names = ['ShaderToolsSmoke', 'RendererSmoke', 'RendererFeaturesSmoke',
         'Renderer2DSmoke', 'SceneGPUSmoke', 'SpriteSmoke']
for configuration in ['Debug', 'Release']:
    hazel.build(configuration, tests=True,
        targets=['MetalRenderer2DQuadSmoke'] + ['Migration' + name for name in names]
            + ['MetalShaderSmoke', 'MetalM1Smoke'])
```

```sh
python3 scripts/hazel.py test --config Debug --profile native --only ShaderToolsSmoke --only RendererSmoke --only RendererFeaturesSmoke --only Renderer2DSmoke --only SceneGPUSmoke --only SpriteSmoke
python3 scripts/hazel.py test --config Release --profile native --only ShaderToolsSmoke --only RendererSmoke --only RendererFeaturesSmoke --only Renderer2DSmoke --only SceneGPUSmoke --only SpriteSmoke
python3 tests/macos/sdk_workflow.py
python3 tools/agents/check_public_docs.py --root .
```

MetalShaderSmoke uses an isolated writable copy of the shader resource bundle and
fresh user-data directory per configuration. Raw logs, readbacks and the source
hash manifest will be linked from the results record at acceptance.

## Native measurements

Host: Apple M4 arm64, macOS 27.3 (26C5035r), Xcode 27.3 (27C5020n).
Final Debug/Release runs each complete 22 native submissions, with 19 allocator
slot reuses, peak in-flight three and zero Metal errors. Nine saved fixtures
independently check 6,272 pixels per configuration: five B3 fixtures through the
factory plus 32×16 / 16×32 resized targets, an 8×8 destroyed target and an 8×8
busy-retry target. Maximum absolute color/depth error is zero; signed IDs and
clears are exact, including INT32_MIN clear and INT32_MAX drawn ID. All saved
Debug/Release byte hashes match. Earlier returned CPU readbacks remain stable
after twelve successive normal/mirrored submissions and resource reuse.

The lifecycle test closes M1's one-use native queue gate around three accepted
submissions. Resize, generation replacement, framebuffer destruction, texture
replacement, cancellation, invalid operations and two failed busy submissions
all return while GPU progress is blocked. Weak attachment/texture references
prove those three submissions retain their original generations until completion.
The test holds multiple unbound framebuffer packets, submits one while another
exists, and rejects reading a prior generation after a new Bind. Cancelled,
never-submitted attachments expire when their owner is destroyed.

After releasing that gate, polling completes all three and their retired
attachment generations expire. The second busy packet retries unchanged,
submits successfully, and its framebuffer is destroyed before host retirement.
One unretired submission is recorded at shutdown; drain publishes its exact
clear-only readback and releases the generation and shader ownership. Unlike B3's
three-gated-submissions shutdown fixture, B4 uses the single gate to test the new
resize/destruction paths and an actual busy-packet retry, then verifies shutdown
retirement separately. The final unretired submission is not claimed to have
been GPU-gated at shutdown.

Peak frame-owned native bytes are 86,592; peak allocator bytes 7,745,064. Remaining
pending-frame bytes are zero after drain. M1's 1 MiB / 16 MiB budgets remain.
Framebuffer-owned current generations can remain alive as ordinary resources
outside pending-frame accounting, as can external textures. Driver/library and
argument-table implementation storage remain excluded. This is not a total GPU
memory/leak counter. The peak differs from B3 because the gated target sizes and
workloads differ; no performance or memory improvement is claimed.

The ordered fixture still records four admitted quads, two successful runs/draws
and 1,024 vertex bytes. Capacity retains eight rendered quads in one 48-index draw
and the ninth-quad rejection/recovery contract. Camera versions, texture updates
before flush, sampler/UV orientation, exact signed IDs, depth LESS, clear depth
one, mirrored geometry and immutable argument/uniform/vertex versions remain
covered by the unchanged shared packing and encoder.

All ten shader packages remain byte-identical to B1/B3. Quad32 SHA-256 remains
`9b27836ce452e162809395fd22dbf07e424f79d1ecbd7b4ab4c2fd4ef8747a78`.
Production MetalShaderSmoke loading/cache/ownership passes Debug/Release. No
GLSL, lighting model, frozen reference, shader compiler or cache identity change.

Initial diagnostics are retained: moving the test away from MetalQuadFrame's
transitive includes required an explicit MetalTexture include. The first harness
attempt tried to use M1's queue gate twice and was rejected; the final test uses
one gate and does not change M1's diagnostic contract. The compiler identified a
new size_t-to-GLM-index narrowing warning; the four-component clear validation
loop now uses int, matching GLM's index type. No warning suppression was added.
Final source inspection also closed stale-current readback access after Bind and
made ambient offscreen clear/viewport calls explicitly unavailable.

## Shortest credible path to a visible Metal 2D scene

Proceed next with **one integrated, opt-in visible quad-scene slice**, using this
production framebuffer and the existing Scene/SceneRenderer/Renderer2D submission.
No additional isolated offscreen checkpoint is recommended. The remaining work
is a connected application/pass/presentation dependency, not another shader demo:

1. Add bounded quad-only initialization/shutdown to the existing Renderer mode
   ownership without starting ImGui, text or Renderer3D. In the existing
   SceneRenderer::Render owner, admit an explicitly quad-only scene and call the
   existing DrawWorld2D callback once. The current path enters generic raster state,
   Renderer3D and (for scene-referred rendering) Depth24/stencil radiance targets
   and CompositeWorldTo; these cannot simply be enabled by switching API. Reject
   unsupported scene content up front. Complete only the ordinary texture/white
   quad admission needed by the chosen visible scene, reusing existing decoding,
   resource ownership and packing rather than another scene traversal.
2. Establish the small explicit pass chain needed by that scene's accepted color
   semantics. Preserve the existing WorldPresentation neutral/exposure behavior
   when scene-referred rendering is selected and DisplayOutput's sRGB encoding;
   do not substitute new tone mapping or hand-authored MSL. Qualify explicit native
   output pipeline formats/cache keys: the current artifact/pipeline qualification
   targets RGBA32Float, while MetalSurface's CAMetalLayer is BGRA8Unorm with sRGB
   color space. Enable render-target sampling usage/residency and render-to-sample
   visibility for the source generations. Keep this concrete pass sequence in
   existing owners; a render graph or full renderer port is unnecessary.
3. Connect those passes to MetalContext::SwapBuffers / MetalDevice::Present.
   Present currently creates a separate clear-only drawable pass, which would
   overwrite any scene unless replaced by the explicit output pass. Reuse M1's
   drawable wait/signal/present handling, shared timeline and allocator slots,
   retaining source generations, pass resources and drawable lifetime through
   completion/presentation. Use GPU passes, not CPU readback/display copies.
4. Qualify real drawable extents and 1×/2× resize/minimize/recovery in that same
   visible slice. Lift the experimental 32-pixel target bound with measured
   framebuffer-owner plus pending-generation budgets; the existing 1 MiB frame
   cap and unconditional full readback are unsuitable for ordinary window sizes.
   Make readback explicit/on demand for the visible loop rather than mandatory
   per displayed frame. The first visible scene can retain eight quads; that
   qualification bound need not become the general batching architecture.

Acceptance should show an actual production-owned textured quad scene with camera
motion and texture changes in the normal window loop, correct encoded color/UV
orientation and depth/IDs, resize and detached-generation safety, and positively
presented frames (nonzero presentedTime), alongside offscreen readback regressions.
This must include a real visible observation/capture; zero presentation timestamps
or completed offscreen submissions alone do not demonstrate a visible scene.
General Renderer2D parity, blending/UI, Renderer3D and editor integration can follow
that visible integration instead of blocking it. No such implementation or next
checkpoint is included here.

## Acceptance — PASS

Final native evidence is `build/metal-m2b4-evidence/debug-qualified` and
`release-qualified`; every source/artifact/readback hash, file changed and measured
value is recorded in [metal-m2b4-results.json](metal-m2b4-results.json).

Six focused regressions pass in each configuration: ShaderToolsSmoke (CPU),
SpriteSmoke, RendererSmoke, RendererFeaturesSmoke, Renderer2DSmoke and SceneGPUSmoke
(native Apple OpenGL). Production MetalShaderSmoke also passes both configurations.
Other suites were not rerun. MetalM1Smoke is rebuilt for interface compatibility;
presentation is not executed or claimed. Direct public/native header inclusion
passes C++17 -fblocks -Wall/-Wextra without a PCH. Seven SDK workflow checks pass.
Final builds introduce zero C++ warnings; ten retained archive/Xcode build-phase
warning occurrences remain, with the resolved clear-loop warning documented above.
No warnings are suppressed, and no unrelated cleanup is included.

The development guide documents factory/async framebuffer coverage and retains
experimental limits. The scheduled public-docs health check passes. README,
examples, screenshots, platform prerequisites, backend defaults and CI policy
remain unchanged. Windows/Linux native builds/execution, physical Intel GPU,
CI, visible presentation, general game export and full scene/editor coverage are
NOT RUN. Docker's local daemon is unavailable; no repair loop or CI dispatch is
attempted. Native platform guards and existing Premake macOS source exclusions
are inspected, not claimed as cross-platform execution evidence.

Remaining limits are the explicit tiny fixed-format, single-sample clear/store
profile and eight opaque textured quads per submission; no load-preserved pass,
blending, additional formats, compositing/output/outline support, presentation or
general scene renderer. RGBA8 linear-filter precision and M1's device-loss /
unsignaled-completion timeout-abort policy remain unchanged. No claim of end-user
Metal renderer support is made. No merge or next-stage implementation is included.
