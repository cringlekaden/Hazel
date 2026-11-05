# Native import dependency gate

TinyGLTF **v3.0.1**, commit `0d9ce1a9561688c6c6e7471430d15a50a0b15f1a`,
is integrated as its C11 translation unit. The tag's header still describes itself
as 3.0.0-alpha; the committed provenance identifies the actual inspected bytes.
[Source hashes and original URLs](../../Hazel/vendor/tinygltf3/PROVENANCE.json),
[MIT notice](../../Hazel/vendor/tinygltf3/LICENSE) and bundled numeric-conversion
[notice](../../Hazel/vendor/tinygltf3/NOTICE.md) travel with pristine source.
Filesystem, image, writer and coroutine opt-ins remain off.

MikkTSpace commit `3e895b49d05ea07e4c2133156cfa94369e19e409` is also pristine;
[provenance](../../Hazel/vendor/mikktspace/PROVENANCE.json) records the source and
the original permissive notice remains in both source files. Tangents are returned
per corner, including mirrored UV handedness; callers must split/reindex vertices.

Premake builds producer-only static C libraries. Engine-owned force-included
allocation wrappers cap actual live allocations, count realloc's simultaneous
storage and latch failures. No CMake, system importer, modified vendor source or
runtime parser dependency is introduced.

`ImportDependencySmoke` checks valid GLB bytes, every truncated prefix, invalid
indices, overflowing offsets, exhaustion, 2,000 deterministic mutations, allocation
release and mirrored per-corner Mikk output. Apple Clang 21 Debug and Release pass.
The same corpus passes ASan + UBSan with stack-use-after-return detection under the
selected full Xcode toolchain. LeakSanitizer is unavailable on this macOS host;
the wrapper's live-allocation assertions run in all configurations. MSVC/GCC
qualification uses the canonical CI inventory; results must be recorded separately.
The first MSVC build exposed its missing C `max_align_t`; the engine allocation
wrapper now preserves MSVC x64 malloc alignment explicitly, without vendor edits.

The parser is not the engine validator. The inspected implementation does not
fully enforce its strictness option or fixed-vector field shapes. The adapter must
preflight supported JSON semantics and GLB framing, then common engine validation
must check the copied data. Allocation exhaustion is rejected even if upstream
returns success. Upstream error formatting can retain an invalid message pointer
when its arena is exhausted: consumers read numeric codes/offsets only and supply
owned diagnostics, without dereferencing upstream messages or JSON-path pointers.
