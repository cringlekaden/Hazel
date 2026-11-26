#!/usr/bin/env python3
"""Run native spotlight qualification or a matched cull/map/forward GPU benchmark."""
import argparse
import csv
import hashlib
import json
import math
import os
import shutil
from pathlib import Path
import statistics
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import hazel as hz
from macos.xegtao import pixels, png


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', choices=('Debug', 'Release'), required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--backend', choices=('metal', 'opengl', 'both'), default='both')
    parser.add_argument('--benchmark', action='store_true')
    parser.add_argument('--archives', type=Path, help='Release archives produced by hazel.py package --app all')
    args = parser.parse_args()
    if args.archives and (args.config != 'Release' or args.benchmark):
        parser.error('Archive checks use Release without --benchmark')
    if args.benchmark and args.backend != 'metal':
        parser.error('The phase benchmark needs --backend metal')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        raise RuntimeError('Use a fresh evidence directory')
    resources = hz.ROOT / 'Hazel/Resources'
    sdk = hz.mono_prefix()
    binaries = hz.binaries(args.config)
    env = {k: v for k, v in os.environ.items() if not k.startswith(('HAZEL_', 'MTL_', 'DYLD_'))}
    env.update(DYLD_LIBRARY_PATH=str(sdk / 'lib'), HAZEL_MONO=str(sdk), HAZEL_RESOURCES=str(resources),
               HAZEL_RENDERING_REFERENCE_PROJECT=str(hz.ROOT / 'examples/RenderingReference/RenderingReference-SpotShadows.hproj'))
    if args.benchmark:
        env['HAZEL_SPOT_BENCHMARK'] = '1'
    else:
        env.update(MTL_DEBUG_LAYER='1', MTL_SHADER_VALIDATION='1')
    report = dict(config=args.config, validation=not args.benchmark, head=hz.output(['git', 'rev-parse', 'HEAD']),
                  sourceSHA256=hashlib.sha256((resources / 'shaders/ForwardPBR.glsl').read_bytes()).hexdigest(),
                  artifactSHA256=hashlib.sha256((resources / 'shaders/metal/ForwardPBR.hzmetal').read_bytes()).hexdigest(),
                  cases={}, status='INCOMPLETE')

    def run(name, base, arguments):
        command = [str(v) for v in [binaries / base / base, *arguments]]
        with (output / (name + '.log')).open('w') as log:
            checked = subprocess.run(command, cwd=hz.ROOT, env={**env, 'HAZEL_DATA': str(output / (name + '-profile'))},
                                     stdout=log, stderr=subprocess.STDOUT, timeout=300)
        log = (output / (name + '.log')).read_text()
        report['cases'][name] = dict(command=command, exitCode=checked.returncode,
                                     summary=[line for line in log.splitlines() if line.startswith(('SPOT', 'PASS', 'FAIL', 'Renderer:'))])
        if checked.returncode:
            raise RuntimeError(name + ' failed; see ' + str(output / (name + '.log')))

    try:
        if args.archives:
            verify_archives(args.archives.resolve(), output, binaries, sdk, env, report)
            report['status'] = 'PASS'
            print(report['status'])
            return
        if args.backend in ('both', 'metal'):
            base = 'MetalRenderer2DQuadSmoke'
            run('metal', base, [resources, output / 'metal', '--spot-shadows'])
            if not args.benchmark:
                for mode in ('spot-shadow-lifetime', 'spot-shadow-taa-lifetime'):
                    run(mode, base, [resources, output / mode, '--' + mode])
        if args.backend in ('both', 'opengl'):
            run('opengl', 'MigrationRenderer2DSmoke', ['--spot-shadows', output / 'opengl'])
        if args.benchmark:
            groups = {}
            for row in csv.DictReader((output / 'metal/benchmark.csv').read_text().splitlines()):
                key = '/'.join(row[k] for k in ('spots', 'side', 'enabled'))
                groups.setdefault(key, []).append(row)
            summary = {}
            for key, rows in groups.items():
                summary[key] = dict(atlasBytes=int(rows[0]['atlas_bytes']))
                for field in ('cull_ms', 'spot_ms', 'forward_ms'):
                    values = sorted(float(row[field]) for row in rows)
                    summary[key][field] = dict(median=statistics.median(values), p95=values[math.ceil(.95 * len(values)) - 1])
            report['benchmark'] = dict(width=1280, height=720, warmups=5, samples=15, cases=summary)
            report['status'] = 'MEASURED'
        else:
            core = dict.fromkeys(('hard', 'masked', 'slope', 'unshadowed'), (321, 239))
            sizes = {**core, 'motion-0': (321, 239), 'motion-1': (321, 239), 'motion-2': (353, 257),
                     'motion-3': (353, 257), 'study': (960, 540), 'combined': (960, 540)}
            for backend in ('metal', 'opengl'):
                if backend not in report['cases']:
                    continue
                for name, (width, height) in sizes.items():
                    source = output / backend / (name + '.bin')
                    values = pixels(source)
                    if len(values) != width * height:
                        raise RuntimeError('Wrong capture extent for ' + str(source))
                    png(source.with_suffix('.png'), width, height, values)
            if args.backend == 'both':
                report['parity'] = {}
                for name in (*core, 'study', 'combined'):
                    a = pixels(output / 'metal' / (name + '.bin'))
                    b = pixels(output / 'opengl' / (name + '.bin'))
                    errors = sorted(max(abs(x[c] - y[c]) for c in range(3)) for x, y in zip(a, b))
                    report['parity'][name] = dict(pixels=len(errors), idMismatches=sum(x[4] != y[4] for x, y in zip(a, b)),
                        maxDepth=max(abs(x[5] - y[5]) for x, y in zip(a, b)), maxRGB=errors[-1],
                        p99RGB=errors[math.ceil(.99 * len(errors)) - 1], pixelsAbove003=sum(d > .003 for d in errors))
            report['status'] = 'PASS'
    finally:
        (output / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    print(report['status'])


def verify_archives(archives, output, binaries, sdk, env, report, mode="spot", offscreen_only=False, probe_env=None):
    from internal.package_tests import extract_verified
    from macos.packaging import relocate_executable, sign
    from macos.qualification import isolation_command, native_console

    if not offscreen_only:
        native_console(output / 'console.json')
    clean = {k: v for k, v in env.items() if not k.startswith(('HAZEL_', 'DYLD_'))}
    clean.update(PATH='/usr/bin:/bin', DEVELOPER_DIR='/nonexistent', MTL_DEBUG_LAYER='1', MTL_SHADER_VALIDATION='1')
    with tempfile.TemporaryDirectory(prefix=f'Hazel {mode} shadow relocation ü ') as temporary:
        working = Path(temporary)

        def run(name, command, project=None, extra_env=None):
            variables = {**clean, 'HAZEL_DATA': str(working / (name + '-profile')),
                         'HAZEL_SAVE_ROOT': str(working / (name + '-saves'))}
            if extra_env:
                variables.update(extra_env)
            if project:
                variables['HAZEL_RENDERING_REFERENCE_PROJECT'] = str(project)
            with (output / (name + '.log')).open('w') as log:
                result = subprocess.run(isolation_command(command, sdk, metal=True), cwd=working, env=variables,
                                        stdout=log, stderr=subprocess.STDOUT, timeout=120)
            report['cases'][name] = dict(exitCode=result.returncode)
            if result.returncode:
                raise RuntimeError(name + ' failed; see ' + str(output / (name + '.log')))

        for app in ('Nutella', 'Hazelnut'):
            root = extract_verified(archives / (app + '-macos-arm64-Release.zip'), working / app)
            bundle = root / (app + '.app')
            contents = bundle / 'Contents'
            resources = contents / 'Resources'
            subprocess.run(['codesign', '--verify', '--deep', '--strict', str(bundle)], check=True, capture_output=True)
            metadata = json.loads((resources / 'build.json').read_text())
            if list(resources.rglob('*.metal')) or any('PCSS' in name for name in metadata['metal_shaders']):
                raise RuntimeError('Unexpected shader sources or experimental artifacts in ' + app)
            for name, digest in metadata['metal_shaders'].items():
                if hashlib.sha256((resources / 'shaders/metal' / name).read_bytes()).hexdigest() != digest:
                    raise RuntimeError('Package shader digest differs for ' + name)
            if metadata['metal_shaders'].get('ForwardPBR.hzmetal') != report['artifactSHA256']:
                raise RuntimeError('Package does not contain the verified forward shader')
            report['cases'][app + '-inventory'] = dict(exitCode=0, artifacts=len(metadata['metal_shaders']))
            project = next((resources / 'Project').glob('*.hproj'))
            if not offscreen_only:
                for backend in ('metal', 'opengl'):
                    options = ['--no-restore'] if app == 'Hazelnut' else []
                    run(app + '-' + backend, [contents / 'MacOS' / app, '--project', project,
                                             '--' + backend, '--smoke-test', *options])
            if app == 'Nutella':
                probe = working / 'Probe/MacOS/ShadowProbe'
                probe.parent.mkdir(parents=True)
                shutil.copy2(binaries / 'MetalRenderer2DQuadSmoke/MetalRenderer2DQuadSmoke', probe)
                shutil.copytree(contents / 'Frameworks', probe.parent.parent / 'Frameworks')
                relocate_executable(probe)
                sign(probe)
                source_free = working / 'Source-free Resources'
                shutil.copytree(resources, source_free, ignore=shutil.ignore_patterns('*.glsl', '*.metal'))
                descriptor = source_free / 'Project' / project.name
                run('metal-source-free', [probe, source_free, working / 'source-free-captures', f'--{mode}-shadows'], descriptor, probe_env)


if __name__ == '__main__':
    main()
