#!/usr/bin/env python3
"""Build, relocate and dynamically audit the maintained Mono embedding fixture."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
FIXTURE = ROOT / 'tests/macos/feasibility/mono'
DEFAULT_SDK = ROOT / 'build/dependencies/mono/macos-arm64/sdk'
EVIDENCE = ROOT / 'build/testing/macos/mono'


def run(args, **kwargs):
    print('+', ' '.join(map(str, args)), flush=True)
    return subprocess.run(list(map(str, args)), check=True, text=True, **kwargs)


def output(args):
    return subprocess.check_output(list(map(str, args)), text=True).strip()


def compile_managed(layout):
    sdk = layout / 'sdk'
    managed = layout / 'managed'; managed.mkdir()
    mono = [sdk / 'bin/mono', '--config', sdk / 'etc/mono/config', sdk / 'lib/mono/4.5/mcs.exe', '-sdk:4.7.2']
    environment = os.environ.copy()
    environment['MONO_PATH'] = str(sdk / 'lib/mono/4.5')
    run([*mono, '-target:library', '-out:' + str(managed / 'MonoEmbeddingFixture.dll'), FIXTURE / 'ManagedFixture.cs'], env=environment)
    sources = sorted((ROOT / 'Hazel-ScriptCore/Source').rglob('*.cs')) + sorted((ROOT / 'Hazel-ScriptCore/Properties').rglob('*.cs'))
    run([*mono, '-target:library', '-optimize+', '-r:System.Core',
         '-out:' + str(managed / 'Hazel-ScriptCore.dll'), *sources], env=environment)


def compile_host(layout):
    sdk = layout / 'sdk'
    binary = layout / 'bin/MonoEmbeddingProbe'; binary.parent.mkdir()
    run(['xcrun', 'clang++', '-std=c++17', '-arch', 'arm64', '-mmacosx-version-min=27.0',
         '-I' + str(sdk / 'include/mono-2.0'), FIXTURE / 'host.cpp',
         '-L' + str(sdk / 'lib'), '-lmonosgen-2.0',
         '-Wl,-rpath,@executable_path/../sdk/lib', '-o', binary])
    run(['codesign', '--force', '--sign', '-', binary])
    return binary


def audit_and_run(layout):
    binary = layout / 'bin/MonoEmbeddingProbe'
    if output(['lipo', '-archs', binary]).split() != ['arm64']:
        raise RuntimeError('Embedding host is not native arm64')
    linked = output(['otool', '-L', binary])
    loads = output(['otool', '-l', binary])
    if '@rpath/libmonosgen-2.0.1.dylib' not in linked or '@executable_path/../sdk/lib' not in loads:
        raise RuntimeError('Embedding host lacks relocatable Mono load commands')
    environment = {key: value for key, value in os.environ.items()
                   if key not in ('DYLD_LIBRARY_PATH', 'DYLD_FALLBACK_LIBRARY_PATH', 'MONO_PATH', 'MONO_CFG_DIR')}
    environment['DYLD_PRINT_LIBRARIES'] = '1'
    print('+', binary, flush=True)
    with tempfile.TemporaryDirectory(prefix='Hazel Mono cwd ') as unrelated_cwd:
        result = subprocess.run([str(binary)], cwd=unrelated_cwd, env=environment,
                                capture_output=True, text=True)
    print(result.stdout, end='')
    dynamic = result.stderr
    if result.returncode:
        raise RuntimeError('Relocated embedding host failed (' + str(result.returncode) + '):\n' + dynamic)
    forbidden = [str(ROOT), '/opt/homebrew', '/usr/local', '/Library/Developer']
    violations = [line for line in dynamic.splitlines() if any(value in line for value in forbidden)]
    if violations: raise RuntimeError('Relocated process loaded a forbidden producer path:\n' + '\n'.join(violations))
    loaded = [line for line in dynamic.splitlines() if 'dyld' in line and ('dylib' in line or 'framework' in line)]
    if not any('libmonosgen-2.0.1.dylib' in line for line in loaded):
        raise RuntimeError('Dynamic audit did not observe the staged Mono runtime')
    return {'stdout': result.stdout, 'dynamic_library_lines': loaded,
            'otool_dependencies': linked.splitlines(), 'otool_load_commands': loads.splitlines()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', type=Path, default=DEFAULT_SDK)
    args = parser.parse_args()
    sdk = args.sdk.resolve()
    if not (sdk / 'lib/libmonosgen-2.0.dylib').is_file(): raise RuntimeError('Prepared Mono SDK is missing')
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='mono-layout-', dir=EVIDENCE) as producer:
        producer_layout = Path(producer) / 'layout'
        shutil.copytree(sdk, producer_layout / 'sdk', symlinks=True)
        compile_host(producer_layout)
        with tempfile.TemporaryDirectory(prefix='Hazel Mono Relocation é ') as relocation:
            relocated = Path(relocation) / 'Relocated SDK ü' / 'layout'
            shutil.copytree(producer_layout, relocated, symlinks=True)
            # Compile through the relocated SDK so compiler/profile discovery is
            # part of the relocation proof, not only the embedded runtime.
            compile_managed(relocated)
            evidence = audit_and_run(relocated)
            evidence.update({'relocated_path_had_spaces_and_unicode': True,
                             'host_architecture': 'arm64', 'appdomains': 4,
                             'sdk_manifest': str(sdk.parent / 'candidate-manifest.json')})
            (EVIDENCE / 'embedding-evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print('Evidence:', EVIDENCE / 'embedding-evidence.json')


if __name__ == '__main__': main()
