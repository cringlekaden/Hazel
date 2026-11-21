#!/usr/bin/env python3
"""Produce experimental Metal shader packages offline; runtime needs no compiler."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import hazel as hz


def produce(configuration, destination, sources=None, force=False):
    hz.macos_toolchain()
    sources = sources or hz.ROOT / 'Hazel/Resources/shaders'
    tool = hz.binaries(configuration) / 'MetalShaderTools/MetalShaderTools'
    destination.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env['DYLD_LIBRARY_PATH'] = str(hz.mono_prefix() / 'lib')
    producer = '\n'.join(hz.output(['xcrun', 'metal', '--version']).splitlines()[:2]) + '\nSDK=' + hz.output(['xcrun', '--sdk', 'macosx', '--show-sdk-version']) + '\nmetal3.0 -fno-fast-math -mmacosx-version-min=27.0'
    # Compare producer metadata before publishing it; --check uses this fingerprint.
    (destination / 'producer.txt').write_text(producer)
    results = {}
    for source in sorted(sources.glob('*.glsl')):
        for slots in ([16, 32] if source.stem == 'Renderer2D_Quad' else [32]):
            name = source.stem
            variant = name + (f'-{slots}' if name == 'Renderer2D_Quad' else '')
            args = [name, str(slots), str(source.resolve())]
            check = subprocess.run([str(tool), '--check', *args, str(destination.resolve())], env=env, capture_output=True, text=True)
            if check.returncode == 0 and not force:
                results[variant] = 'HIT'
                continue
            print(f'Rebuilding {variant}: {check.stderr.strip()}', flush=True)
            with tempfile.TemporaryDirectory(prefix='.metal-shader-', dir=destination) as temporary:
                work = Path(temporary)
                (work / 'producer.txt').write_text(producer)
                hz.run([tool, '--translate', *args, work], env=env)
                for stage in ('vertex', 'fragment'):
                    stem = variant + '.' + stage
                    # Stable relative input names avoid checkout paths in emitted libraries.
                    command = ['xcrun', '-sdk', 'macosx', 'metal', '-std=metal3.0', '-fno-fast-math', '-mmacosx-version-min=27.0', '-c', stem + '.metal', '-o', stem + '.air']
                    compiled = subprocess.run(command, cwd=work, capture_output=True, text=True)
                    if compiled.returncode or 'warning:' in compiled.stderr:
                        raise RuntimeError(f'{variant} {stage}: {compiled.stdout}{compiled.stderr}')
                    hz.run(['xcrun', '-sdk', 'macosx', 'metallib', stem + '.air', '-o', stem + '.metallib'], cwd=work)
                hz.run([tool, '--pack', *args, work], env=env)
                # The C++ packer validates before its own atomic rename. This
                # second same-filesystem rename publishes the completed package.
                (work / (variant + '.hzmetal')).replace(destination / (variant + '.hzmetal'))
            results[variant] = 'REBUILT'
    if len(results) != 11:
        raise RuntimeError('Expected ten raster shaders and both quad variants')
    # Dear ImGui uses its upstream MSL directly; no shader source is compiled at runtime.
    source = hz.ROOT / 'Hazel/Resources/shaders/ImGui.metal'
    identity = hashlib.sha256(source.read_bytes() + producer.encode()).hexdigest()
    library = destination / 'ImGui.metallib'
    stamp = destination / 'ImGui.json'
    try:
        cached = json.loads(stamp.read_text())
    except (OSError, ValueError):
        cached = None
    if not force and library.is_file() and cached == {
            'sourceProducer': identity, 'library': hashlib.sha256(library.read_bytes()).hexdigest()}:
        results['ImGui'] = 'HIT'
    else:
        with tempfile.TemporaryDirectory(prefix='.imgui-shader-', dir=destination) as temporary:
            work = Path(temporary)
            (work / 'ImGui.metal').write_bytes(source.read_bytes())
            hz.run(['xcrun', '-sdk', 'macosx', 'metal', '-std=metal3.0', '-fno-fast-math',
                    '-mmacosx-version-min=27.0', '-c', 'ImGui.metal', '-o', 'ImGui.air'], cwd=work)
            hz.run(['xcrun', '-sdk', 'macosx', 'metallib', 'ImGui.air', '-o', 'ImGui.metallib'], cwd=work)
            (work / 'ImGui.metallib').replace(library)
        stamp.write_text(json.dumps({'sourceProducer': identity,
                                    'library': hashlib.sha256(library.read_bytes()).hexdigest()}) + '\n')
        results['ImGui'] = 'REBUILT'
    # XeGTAO is a narrow native compute library; it has no runtime compiler dependency.
    source = sources / 'XeGTAO/XeGTAO.metal'
    identity = hashlib.sha256(source.read_bytes() + producer.encode()).hexdigest()
    library = destination / 'XeGTAO.metallib'
    stamp = destination / 'XeGTAO.json'
    try:
        cached = json.loads(stamp.read_text())
    except (OSError, ValueError):
        cached = None
    if not force and library.is_file() and cached == {
            'sourceProducer': identity, 'library': hashlib.sha256(library.read_bytes()).hexdigest()}:
        results['XeGTAO'] = 'HIT'
    else:
        with tempfile.TemporaryDirectory(prefix='.xegtao-shader-', dir=destination) as temporary:
            work = Path(temporary)
            (work / 'XeGTAO.metal').write_bytes(source.read_bytes())
            compiled = subprocess.run(['xcrun', '-sdk', 'macosx', 'metal', '-std=metal3.0', '-fno-fast-math',
                    '-mmacosx-version-min=27.0', '-c', 'XeGTAO.metal', '-o', 'XeGTAO.air'], cwd=work, capture_output=True, text=True)
            if compiled.returncode or 'warning:' in compiled.stderr:
                raise RuntimeError(f'XeGTAO: {compiled.stdout}{compiled.stderr}')
            hz.run(['xcrun', '-sdk', 'macosx', 'metallib', 'XeGTAO.air', '-o', 'XeGTAO.metallib'], cwd=work)
            (work / 'XeGTAO.metallib').replace(library)
        stamp.write_text(json.dumps({'sourceProducer': identity,
                                    'library': hashlib.sha256(library.read_bytes()).hexdigest()}) + '\n')
        results['XeGTAO'] = 'REBUILT'
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', choices=['Debug', 'Release'], default='Debug')
    parser.add_argument('--build', action='store_true')
    parser.add_argument('--force', action='store_true', help='Regenerate even valid packages, including native-library failures')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.build:
        hz.build(args.config, tests=True, targets=['MetalShaderTools'])
    print(json.dumps(produce(args.config, args.output.resolve(), force=args.force), indent=2))


if __name__ == '__main__':
    main()
