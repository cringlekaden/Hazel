#!/usr/bin/env python3
"""Run actual pinned shader dependency generators into ignored build output."""
from pathlib import Path
import os
import re
import subprocess
import sys
import shutil
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SOURCES = ROOT / 'build/dependencies/sources'
SHADERC = SOURCES / 'shaderc'
TOOLS = SHADERC / 'third_party/spirv-tools'
HEADERS = SHADERC / 'third_party/spirv-headers/include'
GLSLANG = SHADERC / 'third_party/glslang'
OUTPUT = ROOT / 'build/dependencies/generated'


def run(script, *arguments, cwd=None):
    environment = os.environ.copy()
    environment['SOURCE_DATE_EPOCH'] = subprocess.check_output(['git', '-C', str(SHADERC), 'show', '-s', '--format=%ct', 'HEAD'], text=True).strip()
    subprocess.run([sys.executable, str(script), *map(str, arguments)], cwd=cwd, env=environment, check=True)


def generate(destination):
    output = destination / 'spirv-tools'
    output.mkdir(parents=True, exist_ok=True)
    (destination / 'shaderc').mkdir(exist_ok=True)
    grammar = HEADERS / 'spirv/unified1'
    generator = TOOLS / 'utils/generate_grammar_tables.py'
    core_arguments = [f'--spirv-core-grammar={grammar / "spirv.core.grammar.json"}',
        f'--extinst-debuginfo-grammar={grammar / "extinst.debuginfo.grammar.json"}',
        f'--extinst-cldebuginfo100-grammar={grammar / "extinst.opencl.debuginfo.100.grammar.json"}']
    run(generator, *core_arguments, f'--core-insts-output={output / "core.insts-unified1.inc"}',
        f'--operand-kinds-output={output / "operand.kinds-unified1.inc"}', '--output-language=c++')
    run(generator, *core_arguments, f'--extension-enum-output={output / "extension_enum.inc"}',
        f'--enum-string-mapping-output={output / "enum_string_mapping.inc"}', '--output-language=c++')
    run(generator, f'--extinst-glsl-grammar={grammar / "extinst.glsl.std.450.grammar.json"}',
        f'--glsl-insts-output={output / "glsl.std.450.insts.inc"}', '--output-language=c++')
    run(generator, f'--extinst-opencl-grammar={grammar / "extinst.opencl.std.100.grammar.json"}',
        f'--opencl-insts-output={output / "opencl.std.insts.inc"}')
    for table, prefix in [('spv-amd-shader-explicit-vertex-parameter', ''), ('spv-amd-shader-trinary-minmax', ''),
                         ('spv-amd-gcn-shader', ''), ('spv-amd-shader-ballot', ''), ('debuginfo', ''),
                         ('opencl.debuginfo.100', 'CLDEBUG100_'), ('nonsemantic.shader.debuginfo.100', 'SHDEBUG100_'),
                         ('nonsemantic.clspvreflection', '')]:
        source = grammar / f'extinst.{table}.grammar.json'
        if not source.exists():
            source = TOOLS / 'source' / source.name
        run(generator, f'--extinst-vendor-grammar={source}', f'--vendor-insts-output={output / (table + ".insts.inc")}', f'--vendor-operand-kind-prefix={prefix}')
    for name, table in [('DebugInfo', 'debuginfo'), ('OpenCLDebugInfo100', 'opencl.debuginfo.100'), ('NonSemanticShaderDebugInfo100', 'nonsemantic.shader.debuginfo.100')]:
        run(TOOLS / 'utils/generate_language_headers.py', f'--extinst-grammar={grammar / ("extinst." + table + ".grammar.json")}', f'--extinst-output-path={output / (name + ".h")}')
    run(TOOLS / 'utils/generate_registry_tables.py', f'--xml={HEADERS / "spirv/spir-v.xml"}', f'--generator-output={output / "generators.inc"}')
    run(TOOLS / 'utils/update_build_version.py', TOOLS / 'CHANGES', output / 'build-version.inc', cwd=TOOLS)
    run(SHADERC / 'utils/update_build_version.py', SHADERC, TOOLS, GLSLANG, destination / 'shaderc/build-version.inc', cwd=SHADERC)
    # Same version extraction/substitution as pinned glslang parse_version.cmake;
    # retain its actual template/license and macros without invoking CMake.
    version = re.search(r'#+ *([0-9]+)\.([0-9]+)\.([0-9]+)(?:-([a-zA-Z0-9]+))?', (GLSLANG / 'CHANGES.md').read_text())
    if not version:
        raise RuntimeError('Cannot parse pinned glslang version')
    content = (GLSLANG / 'build_info.h.tmpl').read_text()
    for key, value in zip(('major', 'minor', 'patch', 'flavor'), version.groups()):
        content = content.replace('@' + key + '@', value or '')
    build_info = destination / 'include/glslang/build_info.h'
    build_info.parent.mkdir(parents=True, exist_ok=True)
    build_info.write_text(content)


def main():
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='shader-inputs-', dir=OUTPUT.parent) as directory:
        generated = Path(directory)
        generate(generated)
        for source in generated.rglob('*'):
            if source.is_file():
                target = OUTPUT / source.relative_to(generated)
                target.parent.mkdir(parents=True, exist_ok=True)
                if not target.exists() or target.read_bytes() != source.read_bytes():
                    shutil.copyfile(source, target)
    print(f'Generated actual pinned shader inputs: {OUTPUT}', flush=True)


if __name__ == '__main__':
    main()
