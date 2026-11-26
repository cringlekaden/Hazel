#!/usr/bin/env python3
"""Run point-shadow references, native GPU timings, or relocated archive checks."""
import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import statistics
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import hazel as hz
from macos.local_lighting_presented import presented_timings
from macos.qualification import native_console
from macos.spot_shadows import verify_archives
from macos.xegtao import pixels, png


def stats(values):
    values = sorted(values)
    return dict(median=statistics.median(values), p95=values[math.ceil(.95 * len(values)) - 1])


def benchmark(path):
    groups = {}
    with path.open() as stream:
        for row in csv.DictReader(stream):
            groups.setdefault((row['side'], row['enabled']), []).append(row)
    summary = {}
    for side in ('256', '512'):
        control, enabled = (groups[side, state] for state in ('0', '1'))
        if len(control) != 15 or len(enabled) != 15:
            raise RuntimeError('Incomplete matched phase timings')
        summary[side] = {
            'atlasBytes': int(enabled[0]['atlas_bytes']),
            **{key: stats(float(row[key]) for row in enabled) for key in ('cull_ms', 'point_ms', 'forward_ms')},
            'controlForward': stats(float(row['forward_ms']) for row in control),
            'pairedForwardDelta': stats(float(a['forward_ms']) - float(b['forward_ms']) for a, b in zip(enabled, control)),
        }
    return dict(width=1280, height=720, warmups=5, samples=15, cases=summary)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', choices=('Debug', 'Release'), required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--backend', choices=('metal', 'opengl', 'both'), default='both')
    parser.add_argument('--benchmark', action='store_true')
    parser.add_argument('--archives', type=Path)
    parser.add_argument('--offscreen-only', action='store_true', help='Verify archive inventories and the relocated source-free Metal probe')
    parser.add_argument('--presented', action='store_true')
    parser.add_argument('--unshadowed-baseline', metavar='REF', help='Compare disabled point maps against an earlier forward shader')
    args = parser.parse_args()
    if args.benchmark and (args.archives or args.presented or args.backend != 'metal'):
        parser.error('Use --backend metal for the offscreen benchmark')
    if args.offscreen_only and not args.archives:
        parser.error('Use --archives for offscreen relocation checks')
    if args.unshadowed_baseline and not args.benchmark:
        parser.error('Use --benchmark to compare the unshadowed path')
    if (args.archives or args.presented) and args.config != 'Release':
        parser.error('Use Release for archive and presentation checks')
    if args.presented and args.archives:
        parser.error('Presentation and archive checks need separate output directories')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        raise RuntimeError('Use a fresh evidence directory')
    resources = hz.ROOT / 'Hazel/Resources'
    sdk = hz.mono_prefix()
    binaries = hz.binaries(args.config)
    env = {k: v for k, v in os.environ.items() if not k.startswith(('HAZEL_', 'MTL_', 'DYLD_'))}
    env.update(DYLD_LIBRARY_PATH=str(sdk / 'lib'), HAZEL_MONO=str(sdk), HAZEL_RESOURCES=str(resources),
               HAZEL_RENDERING_REFERENCE_PROJECT=str(hz.ROOT / 'examples/RenderingReference/RenderingReference-PointShadows.hproj'))
    if args.benchmark:
        env['HAZEL_POINT_BENCHMARK'] = '1'
    else:
        env.update(MTL_DEBUG_LAYER='1', MTL_SHADER_VALIDATION='1')
    report = dict(config=args.config, validation=not args.benchmark, head=hz.output(['git', 'rev-parse', 'HEAD']),
                  sourceSHA256=hashlib.sha256((resources / 'shaders/ForwardPBR.glsl').read_bytes()).hexdigest(),
                  artifactSHA256=hashlib.sha256((resources / 'shaders/metal/ForwardPBR.hzmetal').read_bytes()).hexdigest(),
                  cases={}, status='INCOMPLETE')

    def run(name, executable, *arguments):
        command = [str(v) for v in (binaries / executable / executable, *arguments)]
        log = output / (name + '.log')
        with log.open('w') as stream:
            result = subprocess.run(command, cwd=hz.ROOT, env={**env, 'HAZEL_DATA': str(output / (name + '-profile'))},
                                    stdout=stream, stderr=subprocess.STDOUT, timeout=300)
        lines = log.read_text(errors='replace').splitlines()
        report['cases'][name] = dict(command=command, exitCode=result.returncode,
            summary=[line for line in lines if line.startswith(('POINT', 'PASS', 'FAIL', 'Renderer:', 'SHADOW_EDITOR'))])
        if result.returncode:
            raise RuntimeError(name + ' failed; see ' + str(log))
        print('PASS', name, flush=True)

    try:
        if args.archives:
            verify_archives(args.archives.resolve(), output, binaries, sdk, env, report, mode='point', offscreen_only=args.offscreen_only)
        elif args.presented:
            native_console(output / 'console.json')
            run('editor-3200x1800', 'MetalM1Smoke', output / 'editor', '--point-shadow-editor-settings', resources)
            for key in ('MTL_DEBUG_LAYER', 'MTL_SHADER_VALIDATION'):
                env.pop(key, None)
            env.update(HAZEL_LOCAL_PRESENT_BENCHMARK='1', HAZEL_METAL_TRACE='1')
            source = Path(env['HAZEL_RENDERING_REFERENCE_PROJECT'])
            project = output / 'project'
            shutil.copytree(source.parent / 'Assets', project / 'Assets')
            shutil.copy2(source, project / source.name)
            scene = project / 'Assets/Scenes/PointShadows_v1.hazel'
            enabled = scene.read_text()
            report['presented'] = {}
            for name in ('control', 'enabled'):
                scene.write_text(enabled.replace('      CastShadows: true', '      CastShadows: false', 1)
                    if name == 'control' else enabled)
                run(name, 'MetalM1Smoke', output / name, '--benchmark-metal', resources, project / source.name, '-1')
                report['presented'][name] = presented_timings(output / (name + '.log'), name)
        else:
            if args.backend in ('metal', 'both'):
                run('metal', 'MetalRenderer2DQuadSmoke', resources, output / 'metal', '--point-shadows')
                if not args.benchmark:
                    for mode in ('point-shadow-lifetime', 'point-shadow-taa-lifetime'):
                        run(mode, 'MetalRenderer2DQuadSmoke', resources, output / mode, '--' + mode)
            if args.backend in ('opengl', 'both'):
                run('opengl', 'MigrationRenderer2DSmoke', '--point-shadows', output / 'opengl')
            if args.benchmark:
                report['benchmark'] = benchmark(output / 'metal/benchmark.csv')
                if args.unshadowed_baseline:
                    baseline = output / 'baseline-resources'
                    baseline.mkdir()
                    for child in resources.iterdir():
                        if child.name == 'shaders':
                            shutil.copytree(child, baseline / child.name)
                        else:
                            (baseline / child.name).symlink_to(child, target_is_directory=child.is_dir())
                    for path in ('shaders/ForwardPBR.glsl', 'shaders/metal/ForwardPBR.hzmetal'):
                        data = subprocess.check_output(['git', 'show', args.unshadowed_baseline + ':Hazel/Resources/' + path], cwd=hz.ROOT)
                        (baseline / path).write_bytes(data)
                    run('metal-baseline', 'MetalRenderer2DQuadSmoke', baseline, output / 'metal-baseline', '--point-shadows')
                    old = benchmark(output / 'metal-baseline/benchmark.csv')['cases']
                    report['unshadowedBaseline'] = dict(revision=hz.output(['git', 'rev-parse', args.unshadowed_baseline]),
                        cases={side: case['controlForward'] for side, case in old.items()})
            else:
                backends = [b for b in ('metal', 'opengl') if b in report['cases']]
                report['parity'] = {}
                for path in sorted((output / backends[0]).glob('*.bin')):
                    size = (353, 257) if path.stem in ('motion-2', 'motion-3') else (960, 540) if path.stem.startswith('study') else (321, 239)
                    images = [pixels(output / b / path.name) for b in backends]
                    for backend, values in zip(backends, images):
                        if len(values) != math.prod(size):
                            raise RuntimeError('Wrong capture extent for ' + path.name)
                        png(output / backend / path.with_suffix('.png').name, *size, values)
                    if len(images) == 2 and not path.stem.startswith('motion-'):
                        pairs = list(zip(*images))
                        errors = [max(abs(a[c] - b[c]) for c in range(3)) for a, b in pairs]
                        mismatches = sum(a[4] != b[4] for a, b in pairs)
                        depth = max(abs(a[5] - b[5]) for a, b in pairs)
                        report['parity'][path.stem] = dict(idMismatches=mismatches, maxDepth=depth, maxRGB=max(errors),
                            p99RGB=sorted(errors)[math.ceil(.99 * len(errors)) - 1], pixelsAbove003=sum(e > .003 for e in errors))
                        if not path.stem.startswith('study') and (mismatches or depth > 1e-6 or max(errors) > .003):
                            raise RuntimeError('Controlled point shadow parity failed for ' + path.name)
                        if path.stem.startswith('study') and (depth > 1e-6 or sum(e > .003 for e in errors) > .001 * len(errors)):
                            raise RuntimeError('Authored point shadow parity failed for ' + path.name)
                if len(backends) == 2 and 'study' in report['parity']:
                    def mismatched_ids(name):
                        return [(i, a[4], b[4]) for i, (a, b) in enumerate(zip(
                            *(pixels(output / backend / (name + '.bin')) for backend in backends))) if a[4] != b[4]]
                    if mismatched_ids('study') != mismatched_ids('study-clear'):
                        raise RuntimeError('Point shadow state changed cross-backend authored ID differences')
        report['status'] = 'MEASURED' if args.benchmark else 'PASS offscreen relocation' if args.offscreen_only else 'PASS'
    finally:
        (output / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    print(report['status'])


if __name__ == '__main__':
    main()
