---
name: hazel-cpp-integrity
description: Check changed Hazel C++ headers, includes, compiler warnings, and portability. Use for C++ interface edits or warning cleanup; skip for docs-only work.
---

Use `docs/agents/STYLE.md` and `docs/agents/QUALITY.md` only where needed.

1. Inspect changed `.h/.cpp/.mm` files and callers. Match existing adjacent naming, formatting and include style; **do not import Filament's separate naming/formatting rules**.
2. Keep headers self-contained: include what is used; forward-declare only when complete type unnecessary; avoid unnecessary public dependencies, include cycles, hidden include-order dependencies, and native Apple types in portable headers.
3. Check lifetime and interface boundaries: const-correctness, ownership, virtual destructors, move/copy contracts, error reporting, integer narrowing, overflow, ABI/layout, and macro collisions across Windows/macOS/Linux.
4. For new warnings: capture compiler/config/file/line, fix root cause, verify targeted Debug and Release where relevant. Don't blanket-suppress or globally enable `-Werror`; distinguish third-party and baseline warnings.
5. Compile the smallest affected target; for header-facing changes, verify at least one direct-include consumer/translation unit and affected platform build if available. A header-only check is selective verification, not full-platform certification.
6. Report concrete violations with locations, tests performed, unavailable checks; no broad mechanical reorder or mass format unless requested.
