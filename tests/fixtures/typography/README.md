# CPU typography fixtures

`corpus.tsv` is Hazel-authored UTF-8 test material (repository license), covering
English/French/German, composed/decomposed accents, Greek/Cyrillic, Arabic joining
and Arabic/Latin/numbers, Hebrew/Latin/numbers, punctuation, tab/NBSP, ligatures
and a long word. This is a representative CPU-layout corpus, not a rendering or
language certification. Reader-reviewed language typography remains an integration
acceptance task. CJK, color emoji, vertical and variable typography are excluded.

One added static face is needed because shipped Open Sans does not cover the
Arabic/Hebrew fixture. **Unmodified DejaVu Sans 2.37**, 757,076 bytes, is taken from
the project's [official single-face archive](https://github.com/dejavu-fonts/dejavu-fonts/releases/download/version_2_37/dejavu-sans-ttf-2.37.zip).
The [upstream download page](https://dejavu-fonts.github.io/Download.html) publishes
archive SHA-256 `5c6e497a2f36552cb5ffb112c413a6af39c0f3c47653662b90b4fa6499822fd7`.

| File | SHA-256 |
| --- | --- |
| DejaVuSans.ttf | `7da195a74c55bef988d0d48f9508bd5d849425c1770dba5d7bfc6ce9ed848954` |
| LICENSE-DejaVu.txt | `7a083b136e64d064794c3419751e5c7dd10d2f64c108fe5ba161eae5e5958a93` |

The complete archive LICENSE is retained byte-for-byte, including Bitstream Vera,
Arev and contributed-glyph notices. Redistribution with notices is permitted;
modified-font naming and standalone-sale restrictions apply. No face is modified,
subsetted or installed as a system font. Sandbox’s explicitly authored UI Tutorial
now redistributes these exact bytes as `Assets/Fonts/Corpus.ttf`, with the complete
notice; other games and application resources do not implicitly include it. Input remains
below Hazel's existing 4 MiB limit. Latin/Greek/Cyrillic source coverage in engine
Open Sans is separately exercised by accepted-resource integration tests.

The regression runner copies Unicode 17 `GraphemeBreakTest.txt` and
`LineBreakTest.txt` from the pinned libunibreak checkout, and `BidiCharacterTest.txt`
from pinned SheenBidi into its private working directory. Their original headers
and Unicode notices remain intact. Unicode License V3 is in
`scripts/dependencies/licenses/typography/LICENSE-Unicode.txt`. Builds run none of
the upstream generators. Conformance tests check dependency behavior independently
of Hazel's selected-script/layout policy, which is tested separately.

The test corpora/conformance files remain outside runtime Resources/Assets and
are not implicitly included in application/game exports. The explicit Sandbox
tutorial face above is a project-owned dependency, not a test-directory export.

## Run the CPU check

Build with `python3 scripts/hazel.py build --config Release --tests` first. A fixture-directory argument alone is insufficient: accepted-resource checks also load cwd-relative `assets/fonts/opensans`. From the checkout, this display-free Linux command reproduces the runner layout without touching project/profile state:

```sh
python3 - <<'CHECK'
import subprocess, sys, tempfile
from pathlib import Path
sys.path.insert(0, 'scripts')
import hazel
from internal.tests import typography_inputs
with tempfile.TemporaryDirectory(prefix='Hazel-typography-') as directory:
    work = Path(directory)
    hazel.copy_tree(hazel.ROOT / 'Hazel/Resources', work / 'assets')
    typography_inputs(work)  # pinned face, corpus, Unicode 17 data + notice checks
    executable = hazel.binaries('Release') / 'MigrationTypographySmoke/MigrationTypographySmoke'
    subprocess.run([str(executable), str(work / 'typography')], cwd=work, check=True)
CHECK
```

On Windows use `python` and the `.exe` executable; the canonical `hazel.py test` command prepares this same private layout automatically on both OSes. No system fonts, extra fixture downloads or display are needed. [Development guide](../../../docs/development.md) describes the broader suites.
