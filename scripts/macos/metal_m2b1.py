#!/usr/bin/env python3
"""Qualify production Metal shader artifacts/cache and source-free relocation."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import hazel as hz
from macos.metal_shaders import produce


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', choices=['Debug', 'Release'], default='Debug')
    parser.add_argument('--build', action='store_true')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    os.chdir(hz.ROOT)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        raise RuntimeError('Use a fresh evidence directory')
    if args.build:
        hz.build(args.config, tests=True, targets=['MetalShaderTools', 'MetalShaderSmoke', 'MetalM2ASmoke', 'MetalM1Smoke'])
    hz.macos_toolchain()
    result = {'configuration': args.config, 'gates': {}}

    def gate(name, value):
        result['gates'][name] = 'PASS' if value else 'FAIL'
        if not value:
            raise RuntimeError(name + ' failed')

    def run(command, name, env=None, cwd=None):
        with (output / (name + '.log')).open('w') as log:
            completed = subprocess.run(list(map(str, command)), stdout=log, stderr=subprocess.STDOUT, env=env, cwd=cwd, timeout=240)
        gate(name, completed.returncode == 0)

    def hashes(directory):
        return {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(directory.glob('*.hzmetal'))}

    def production(directory, sources=None, force=False):
        # Preserve exact producer commands/diagnostics without chat output volume.
        from contextlib import redirect_stdout
        with (output / 'producer.log').open('a') as log, redirect_stdout(log):
            return produce(args.config, directory, sources, force=force)

    try:
        run([sys.executable, hz.ROOT / 'scripts/macos/metal_m2a.py', '--config', args.config, '--output', output / 'qualification'], 'shader-qualification')
        first, second = output / 'producer-first', output / 'producer-second'
        gate('cold-production', all(v == 'REBUILT' for v in production(first).values()))
        expected = hashes(first)
        production(second)
        gate('deterministic-relocated-production', hashes(second) == expected)
        times = {p.name: p.stat().st_mtime_ns for p in first.glob('*.hzmetal')}
        gate('warm-production', all(v == 'HIT' for v in production(first).values()))
        gate('warm-no-rewrite', times == {p.name: p.stat().st_mtime_ns for p in first.glob('*.hzmetal')})
        gate('forced-production', all(v == 'REBUILT' for v in production(first, force=True).values()) and hashes(first) == expected)
        target = first / 'Renderer2D_Line.hzmetal'
        target.write_bytes(b'truncated')
        rebuilt = production(first)
        gate('corrupt-production-recovery', rebuilt['Renderer2D_Line'] == 'REBUILT' and hashes(first) == expected)

        def mutate_field(index, replacement):
            data = target.read_bytes()
            fields, cursor = [], 72
            while cursor < len(data):
                length = struct.unpack_from('<I', data, cursor)[0]
                fields.append(data[cursor + 4:cursor + 4 + length]); cursor += 4 + length
            fields[index] = replacement
            payload = b''.join(struct.pack('<I', len(field)) + field for field in fields)
            target.write_bytes(b'HZMETAL1' + hashlib.sha256(payload).hexdigest().encode() + payload)

        mutate_field(0, b'stale-profile')
        gate('profile-recovery', production(first)['Renderer2D_Line'] == 'REBUILT' and hashes(first) == expected)
        mutate_field(4, b'other-Apple-toolchain')
        gate('producer-invalidation', production(first)['Renderer2D_Line'] == 'REBUILT' and hashes(first) == expected)
        sources = output / 'changed-sources'
        sources.mkdir()
        for path in (hz.ROOT / 'Hazel/Resources/shaders').glob('*.glsl'):
            shutil.copyfile(path, sources / path.name)
        source = sources / 'Renderer2D_Line.glsl'
        source.write_text(source.read_text() + '\n// Identity invalidation fixture\n')
        changed = production(first, sources)
        gate('source-invalidation', changed['Renderer2D_Line'] == 'REBUILT' and hashes(first)['Renderer2D_Line.hzmetal'] != expected['Renderer2D_Line.hzmetal'])
        gate('unchanged-variant-reuse', all(value == 'HIT' for name, value in changed.items() if name != 'Renderer2D_Line'))
        production(first) # Restore canonical source package.
        gate('canonical-source-restored', hashes(first) == expected)
        relocated = output / 'relocated package space é'
        resources = relocated / 'Resources/shaders/metal'
        resources.mkdir(parents=True)
        for package in first.glob('*.hzmetal'):
            shutil.copyfile(package, resources / package.name)
        executable = relocated / 'MetalShaderSmoke'
        shutil.copyfile(hz.binaries(args.config) / 'MetalShaderSmoke/MetalShaderSmoke', executable)
        executable.chmod(0o755)
        libraries = relocated / 'lib'; libraries.mkdir()
        for path in (hz.mono_prefix() / 'lib').glob('libmonosgen*.dylib'):
            shutil.copyfile(path, libraries / path.name)
        env = os.environ.copy()
        env.update(PATH='/nonexistent', DEVELOPER_DIR='/nonexistent', DYLD_LIBRARY_PATH=str(libraries), MTL_DEBUG_LAYER='1', MTL_DEBUG_LAYER_ERROR_MODE='assert', MTL_SHADER_VALIDATION='1')
        gate('source-free-package', not list(relocated.rglob('*.glsl')) and not list(relocated.rglob('*.metal')))
        run([executable, relocated / 'Resources', relocated / 'user'], 'production-loading', env=env, cwd=relocated)
        text = (output / 'production-loading.log').read_text()
        gate('native-validation', not re.search(r'Validation Error|failed assertion|GPU fault', text))
        result['artifactSHA256'] = expected
        result['qualification'] = json.loads((output / 'qualification/results.json').read_text())
        result['rejections'] = len(re.findall(r'PASS rejected ', text))
        result['verdict'] = 'PASS'
    except Exception as error:
        result['verdict'] = 'INCOMPLETE'
        result['error'] = str(error)
        raise
    finally:
        (output / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps({k: v for k, v in result.items() if k in ('verdict', 'gates', 'error', 'rejections')}, indent=2))


if __name__ == '__main__':
    main()
