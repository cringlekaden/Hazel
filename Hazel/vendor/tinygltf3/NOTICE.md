# TinyGLTF v3 producer dependency

Pinned tag v3.0.1 resolves to 0d9ce1a9561688c6c6e7471430d15a50a0b15f1a.
The unchanged header still labels itself v3.0.0-alpha; the tag/commit and byte
hashes identify the acquired API. Upstream source is not patched.

TinyGLTF and its C API are MIT licensed; original notices are in LICENSE and
tiny_gltf_v3.h. The bundled JSON header credits fast_float (2021, fast_float
authors) and Dragonbox-style conversion (2020–2024, Junekey Jeon). Both offer
Boost Software License 1.0 in that header; this integration selects that offered
license for those conversion portions and includes its complete original text
from https://www.boost.org/LICENSE_1_0.txt. Preserve the header's attribution.

Filesystem, stb image decode/write and coroutine support remain disabled. The
GLB adapter owns source I/O and image decoding. A maintained allocation wrapper
bounds all C allocations without modifying these source files. Runtime engine
assets and Nutella do not link this parser.
