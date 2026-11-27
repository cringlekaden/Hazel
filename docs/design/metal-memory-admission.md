# Metal renderer memory admission

## History and baseline

The fixed aggregate limit originated in `9030dd6` as a 1 MiB resource guard
for M1's frame-tagged transfer proof. `3b22ada` increased it to 1 GiB for
runtime rendering. `878303e` moved it into `MetalDevice.h` and raised it to
2 GiB when Retina HDR, AO and TAA outgrew the earlier limit. Both
`MetalDevice::Account` and `MetalFramebuffer::Allocate` guarded the same
renderer resource domain. These were Hazel policy decisions.

At `b776097` the second queued full-effects 3840×2160 frame failed during
world preparation, before presenting or submitting it. The production
drawable path was used with no bulk readback. At rejection, Metal reported
2,255,863,808 allocated bytes, with 12,713,115,648 bytes of recommended
working set and 17,179,869,184 bytes of installed unified memory. Hazel
tracked 2,212,338,984 bytes. The sample below counts unique allocations.
The first frame was gated on the GPU. CPU-owned incoming or idle resources
can still be needed for the next submission.

| Role | Already used by submitted frames | Incoming or idle | Total bytes |
| --- | ---: | ---: | ---: |
| Display generations and smoke fixture | 201,588,736 | 201,613,568 | 403,202,304 |
| World radiance | 201,588,736 | 0 | 201,588,736 |
| TAA jittered target | 201,588,736 | 0 | 201,588,736 |
| TAA motion target | 201,588,736 | 0 | 201,588,736 |
| TAA histories | 167,116,800 | 167,116,800 | 334,233,600 |
| AO depth prepass | 33,947,648 | 33,947,648 | 67,895,296 |
| AO compute textures, two sets | 257,359,872 | 0 | 257,359,872 |
| SSR surface | 471,072,768 | 0 | 471,072,768 |
| Bloom | 45,301,760 | 0 | 45,301,760 |
| Directional, spot and point shadow atlases | 6,881,280 | 0 | 6,881,280 |
| Other frame resources | 21,625,896 | 0 | 21,625,896 |

The old active counter included AO compute textures from all AO owners, so
that row includes incoming AO scratch. The Metal allocator used another
3,681,832 bytes, governed by a separate Hazel limit. Native allocated bytes
provide an independent view that includes allocations outside the tracked
packet and framebuffer inventory. The counters overlap and are evaluated
separately. Apple describes the recommendation as a performance guideline
and reports current device allocations separately.
[Metal working-set guidance](https://developer.apple.com/documentation/metal/mtldevice/recommendedmaxworkingsetsize) ·
[Current allocation statistics](https://developer.apple.com/documentation/metal/mtldevice/currentallocatedsize)

## Policy and lifetime

On a device with working-set guidance, the tracked cap is three quarters
of that guidance and the native allocation guard is seven eighths. Unified
devices additionally cap tracked resources to one third of installed RAM
and native allocations to one half. The gap leaves room for assets, driver
and drawable allocations outside explicit scene packets, while retaining
headroom for CPU memory and other processes. A device without guidance
derives both guards from a conservative 2 GiB fallback working set. Lower
memory systems receive proportionally smaller budgets. The 16 GiB M4 uses
5,726,623,061 tracked bytes and 8,589,934,592 native bytes as ceilings.
These are admission limits, with the operating system and native allocation
results remaining authoritative.

Hazel admits new framebuffer/AO generations and prepared submissions before
publishing or committing them. Retirement and cleanup update telemetry even
under pressure. Dispatch memory-pressure notifications trim unused
framebuffer generations when that framebuffer is next bound. At critical
pressure, new framebuffer, AO scratch and frame-slot buffer allocation can
be deferred. Already-retired compatible resources can be reused. Surface
replacement remains transactional, and rejected presentation packets are
discarded before submit. Renderer-side allocator, extent, sample count and
pixel-count guards remain separate from the aggregate budget. Native buffer
growth also checks Metal's `maxBufferLength`.

XeGTAO now owns one private depth target and one compute scratch set per
view extent. The depth and compute encoders keep the existing all-stage
queue barriers before overwriting earlier data. Submitted packets retain
textures across resize and destruction. Each in-flight pass keeps separate
timing counters until completion. TAA histories, SSR formats, depth,
picking and shader arithmetic are unchanged.

`HAZEL_METAL_MEMORY_TRACE=1` logs per-role allocation snapshots. The term
`other` in a snapshot includes an incoming or idle attachment. Role counts
identify textures rather than frame packets.

## Qualification

The original validated and validation-disabled 4K runs failed on their
second world packet. With device-aware admission and the queue-ordered AO
resources, both Debug and Release completed 30 3840×2160 frames with
TAA, AO, SSR, two reflection probes, all shadows and bloom. The initial
gate held three packets simultaneously. Three-burst submission continued
with the same effects. Each run also rejected an injected critical-pressure
resize, retained the old surface, prepared and cancelled a temporary odd
extent with different settings while three packets were retained, and
verified a camera cut reset. Shutdown recorded zero live attachment
generations, zero tracked in-flight bytes and zero GPU errors.

Peak bytes including transient resize overlap were 2,467,825,496 tracked and
2,605,285,376 native with validation. The regular three-frame queue reported
2,251,990,616 active-frame bytes before the temporary resize. AO used one
128,679,936-byte compute set and one 33,947,648-byte depth target at 4K.
Compared with the rejected second packet, the change eliminates one
duplicate of each, 162,627,584 bytes, while preserving retained display
generations and two TAA histories.

Release timing with validation disabled used the 27 submissions after the
initial gated warm-up. GPU elapsed median/p95 was 82.115/82.586 ms. CPU
presentation preparation and submission median/p95 was 7.189/7.664 ms.
The CPU timing starts at the drawable path and excludes scene recording.
GPU timings cover the full command buffer and may include overlapping
work; they describe this study, without a cross-version speedup claim.
The detached layer reported zero positive presentation timestamps.
System-wide free-memory estimates were 66% before the timing process and
52% immediately after. The critical-pressure rejection was injected
deterministically. No forced system-wide memory exhaustion was used.

Native HDR and TAA Debug/Release regression tests passed. All 144 HDR
captures across Metal and OpenGL matched the previous checkpoint exactly.
Hazelnut's detached-consumer and resize/recovery gates passed in both
configurations with positive presentation timestamps. The Release Nutella
archive passed relocation, Metal source-free rendering, packaged OpenGL,
and clean desktop shutdown. Debug and Release built without new first-party
warnings. Lower-memory Macs and other GPUs were untested.

Reproduce the sustained study using the
[admission fixture](camera-exposure-admission.md) with
`HAZEL_CAMERA_ADMISSION=4k-sustained HAZEL_METAL_MEMORY_TRACE=1`. The
phase-specific image-formation scratch budgets are unchanged. The recording
tracks allocations, not system memory reserved for exclusive GPU use.

[Rejected frame breakdown](evidence/MetalMemoryAdmission_v1/rejected-second-frame.txt) ·
[Timing samples](evidence/MetalMemoryAdmission_v1/4k-frame-timing.csv) ·
[Qualification results](evidence/MetalMemoryAdmission_v1/results.json)
