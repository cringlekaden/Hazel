#!/usr/bin/env python3
"""Reproduce the bounded M2-A shader and offscreen native Metal 4 checkpoint."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import shutil
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import hazel as hz


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', choices=['Debug', 'Release'], default='Debug')
    parser.add_argument('--build', action='store_true')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    os.chdir(hz.ROOT)
    hz.macos_toolchain()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        raise RuntimeError('Use a fresh evidence directory to prevent stale artifacts')
    commands = []
    os.environ["DYLD_LIBRARY_PATH"] = str(hz.mono_prefix() / "lib")

    def run(command, log, env=None, cwd=None):
        commands.append(list(map(str, command)))
        with (output / log).open('w') as stream:
            result = subprocess.run(list(map(str, command)), stdout=stream, stderr=subprocess.STDOUT, env=env, cwd=cwd)
        return result.returncode

    if args.build:
        # Keep the established producer setup and Premake generation; target only this proof.
        code = 'import sys; sys.path.insert(0,"scripts"); import hazel; hazel.build("' + args.config + '",tests=True,targets=["MetalShaderTools", "MetalM2ASmoke"])'
        if run([sys.executable, '-c', code], 'producer.log'):
            raise RuntimeError('Build failed; see producer.log')
    executable = hz.ROOT / f'bin/{args.config}-macos-arm64/MetalM2ASmoke/MetalM2ASmoke'
    artifacts = output / 'shaders'
    translation = run([executable, '--translate', artifacts], 'translation.log')
    offline = {}
    for source in sorted(artifacts.glob('*.metal')):
        air = source.with_suffix('.air')
        library = source.with_suffix('.metallib')
        status = run(['xcrun', '-sdk', 'macosx', 'metal', '-std=metal3.0', '-fno-fast-math', '-mmacosx-version-min=27.0', '-c', source.name, '-o', air.name], source.name + '.compile.log', cwd=artifacts)
        if not status:
            status = run(['xcrun', '-sdk', 'macosx', 'metallib', air.name, '-o', library.name], source.name + '.link.log', cwd=artifacts)
        if 'warning:' in (output / (source.name + '.compile.log')).read_text():
            status = 1
        offline[source.name] = 'PASS' if not status else 'FAIL'
    if translation == 0 and all(v == 'PASS' for v in offline.values()):
        producer = '\n'.join(hz.output(['xcrun', 'metal', '--version']).splitlines()[:2]) + '\nSDK=' + hz.output(['xcrun', '--sdk', 'macosx', '--show-sdk-version']) + '\nmetal3.0 -fno-fast-math -mmacosx-version-min=27.0'
        (artifacts / 'producer.txt').write_text(producer)
        packaged = artifacts / 'Resources/shaders/metal'
        packaged.mkdir(parents=True)
        tool = hz.binaries(args.config) / 'MetalShaderTools/MetalShaderTools'
        for source in sorted((hz.ROOT / 'Hazel/Resources/shaders').glob('*.glsl')):
            for slots in ([16, 32] if source.stem == 'Renderer2D_Quad' else [32]):
                variant = source.stem + (f'-{slots}' if source.stem == 'Renderer2D_Quad' else '')
                if run([tool, '--pack', source.stem, slots, source, artifacts], variant + '.pack.log'):
                    raise RuntimeError('Artifact packing failed: ' + variant)
                shutil.copyfile(artifacts / (variant + '.hzmetal'), packaged / (variant + '.hzmetal'))
    environment = os.environ.copy()
    environment.update(PATH='/nonexistent', DEVELOPER_DIR='/nonexistent', MTL_DEBUG_LAYER='1', MTL_DEBUG_LAYER_ERROR_MODE='assert', MTL_DEBUG_LAYER_WARNING_MODE='nslog', MTL_SHADER_VALIDATION='1')
    native = run([executable, '--native', artifacts], 'native.log', environment) if translation == 0 and all(v == 'PASS' for v in offline.values()) else None
    native_text = (output / 'native.log').read_text() if native is not None else ''
    validation_diagnostics = [line for line in native_text.splitlines()
                              if re.search(r'(error:|warning:|GPU fault|validation error)', line, re.I)]
    if validation_diagnostics:
        native = 1
    inputs = {str(p.relative_to(hz.ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
              for p in (hz.ROOT / 'Hazel/Resources/shaders').glob('*.glsl')}
    inputs['pins'] = json.loads((hz.ROOT / 'scripts/dependencies/shader-tools.json').read_text())
    inputs['worktree_diff_sha256'] = hashlib.sha256(subprocess.check_output(['git', 'diff', 'HEAD'])).hexdigest()
    files = {str(p.relative_to(output)): hashlib.sha256(p.read_bytes()).hexdigest()
             for p in artifacts.glob('*') if p.is_file()}
    result = dict(configuration=args.config, head=hz.output(['git', 'rev-parse', 'HEAD']),
                  translation='PASS' if translation == 0 else 'FAIL', offline=offline,
                  native='PASS' if native == 0 else 'NOT RUN' if native is None else 'FAIL',
                  inputs=inputs, native_draws=re.findall(r"PASS native_draw (.*)", native_text), validation_diagnostics=validation_diagnostics,
                  compiler=hz.output(['xcrun', 'metal', '--version']),
                  device=hz.output(['system_profiler', 'SPDisplaysDataType']), commands=commands, sha256=files)
    (output / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k in ('translation', 'offline', 'native')}, indent=2))
    return 0 if native == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
