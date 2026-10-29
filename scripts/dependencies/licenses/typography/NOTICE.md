# CPU typography dependency notices

Pristine source pins (Git submodules; no build-time downloads/generators):

- HarfBuzz 12.3.2, `b42511e071162fe76102f613a6ccc009726c99af`:
  https://github.com/harfbuzz/harfbuzz/tree/12.3.2 — Old MIT license.
  Exact upstream COPYING and subsidiary notices ship in `licenses/vendor/harfbuzz`.
- libunibreak 8.0, `28a2756b864c343f438cd22537d49d394d4666a5`:
  https://github.com/adah1972/libunibreak/tree/libunibreak_8_0 — zlib-style license.
  Exact upstream LICENCE ships in `licenses/vendor/libunibreak`.
- SheenBidi 3.0.0, `cfe430e7375a7845b679adae9d51dac6deaa8858`:
  https://github.com/Tehreer/SheenBidi/tree/v3.0.0 — Apache-2.0.
  Copyright (C) 2014-2026 Muhammad Tayyab Akram; source headers retain per-file
  notices. Exact upstream LICENSE ships in `licenses/vendor/SheenBidi`.

All three include generated Unicode 17 data. The accompanying unmodified Unicode
License V3 was downloaded from https://www.unicode.org/license.txt;
SHA-256 `e7a93b009565cfce55919a381437ac4db883e9da2126fa28b91d12732bc53d96`.
No fonts or conformance corpora accompany these runtime libraries themselves.
The CPU fixtures and complete DejaVu notice remain under
[tests/fixtures/typography](../../../../tests/fixtures/typography/README.md).
Sandbox's explicitly authored Corpus family independently redistributes the same
pinned face with its [project notice](../../../../examples/Sandbox/Assets/Fonts/Corpus-NOTICE.txt);
that project dependency is included in Sandbox exports. See the current
[dependency and licensing guide](../../../../docs/platforms-and-dependencies.md#licensing-and-provenance).
