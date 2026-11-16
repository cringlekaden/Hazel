# Hazel C++ style, architecture and maintainability (on demand)

This is **Hazel's** convention policy. Current surrounding code takes precedence when conventions vary between older Hazel modules and newer 3D/Metal code. Source ownership should be readable without a framework diagram.

## Readable design

- Preserve current `Hazel` namespace, local types, API names, macros, and `Ref<>`/`Scope<>` patterns where the module uses them. Follow file-local brace/indent, include and naming patterns; avoid wholesale formatting. Header names, class boundaries, and public methods should match their real purpose.
- Choose one authoritative owner per thing: scene submission, render order, GPU resource creation, cache generation, presentation surface and test evidence. Expose a small interface only where multiple concrete consumers exist now, or an unavoidable backend boundary needs it.
- Keep application/UI/editor and GPU backend concerns separated. Cross-platform headers should not leak `MTL*`, Apple/Cocoa, GLFW-native handles, GL object names or platform framework types except at deliberate narrow edges.
- Favor RAII and explicit lifetime: which object owns storage, who borrows it, who waits for GPU/worker completion, how partial construction and failed shutdown recover. `shared_ptr` reference count alone does not prove GPU completion.
- Make errors actionable. Prefer diagnostics with path, shader stage/variant, backend, resource name and reason to silent fallback or distant assertions. Preserve existing fatal-vs-recoverable behavior unless separately authorized.
- Avoid: speculative ECS/task/render frameworks, generic wrappers around one consumer, unnecessary allocators/virtual dispatch in hot paths, widespread macro metaprogramming, duplicate scene traversal, duplicate shader math, compatibility code without a current supported user.

## C++ and dependency hygiene

- Headers include what their declarations truly require; use forward declarations selectively. Test changed headers independently where supported; avoid accidental transitive includes and cycles.
- Keep headers self-contained for Windows/MSVC, Linux/Clang/GCC and macOS/Clang. Respect platform guards and build only native Metal files on macOS through Premake.
- Preserve stable ordering/namespace conventions found in the touched file. Don't impose Filament's strict topological include tooling or clang-format config on Hazel without demonstrated local benefit and an explicit task.
- Check narrowing, signedness, bounds, alignment/packing, integer overflow, thread safety, callback capture, destructor order, exception/error behavior and accidental per-frame allocations.
- Fix new first-party warnings as bugs, note preexisting or vendor noise separately. Avoid broad suppressions and changing global warning policy as a workaround.

## Performance mindset

Performance means predictable frame budgets, bounded memory/latency, good GPU data flow, clean resource lifetime and measurable behavior. Prefer data-oriented iteration where it improves a real hot path; avoid optimizing on intuition or bringing in Filament's specialized allocation policies wholesale. Benchmark matched workloads before reporting wins.
