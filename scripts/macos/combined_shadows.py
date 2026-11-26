#!/usr/bin/env python3
"""Capture the combined directional, spot and point shadow workload on native macOS."""
import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import hazel as hz
from macos.local_lighting_presented import presented_timings
from macos.point_shadows import stats
from macos.qualification import native_console
from macos.spot_shadows import verify_archives
from macos.xegtao import pixels, png


def summarize_timings(path):
    groups = {}
    with path.open() as stream:
        for row in csv.DictReader(stream):
            groups.setdefault((row['width'], row['csm_side'], row['enabled']), {})[row['sample']] = row
    result = {}
    for width in ('1280', '1920'):
        for side in ('256', '512', '1024', '2048'):
            control, shadow = (groups[width, side, state] for state in ('0', '1'))
            if set(control) != set(shadow) or len(shadow) != 15:
                raise RuntimeError('Incomplete combined timing pairs')
            atlas_fields = ('csm_ms', 'spot_ms', 'point_ms')
            frames = list(shadow.values())
            result[f'{width}/{side}'] = dict(
                atlasBytes=int(frames[0]['atlas_bytes']),
                atlasTotal=stats(sum(float(row[key]) for key in atlas_fields) for row in frames),
                **{key: stats(float(row[key]) for row in frames)
                   for key in (*atlas_fields, 'cull_ms', 'forward_ms')},
                controlForward=stats(float(row['forward_ms']) for row in control.values()),
                pairedForwardDelta=stats(float(row['forward_ms']) - float(control[sample]['forward_ms'])
                                         for sample, row in shadow.items()))
    return result


def parity(first, second):
    a, b = pixels(first), pixels(second)
    if len(a) != len(b):
        raise RuntimeError('Capture dimensions differ')
    error = sorted(max(abs(x[c] - y[c]) for c in range(3)) for x, y in zip(a, b))
    return dict(rgb=stats(error), p99RGB=error[math.ceil(.99 * len(error)) - 1], maxRGB=error[-1], above003=sum(d > .003 for d in error),
                ids=sum(x[4] != y[4] for x, y in zip(a, b)),
                maxDepth=max(abs(x[5] - y[5]) for x, y in zip(a, b)))


def edge_profile(shadow, control, row, start, end, descending=False):
    visibility = [shadow[row * 321 + x][0] / control[row * 321 + x][0]
                  if control[row * 321 + x][0] > .012 else None for x in range(start, end + 1)]

    def crossing(level):
        for x, (a, b) in enumerate(zip(visibility, visibility[1:]), start):
            if a is None or b is None:
                continue
            crossed = b <= level < a if descending else a <= level < b
            if crossed:
                return x + (level - a) / (b - a)
        raise RuntimeError(f'Missing {level} edge crossing on row {row}')

    return abs(crossing(.9) - crossing(.1)), crossing(.5)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', required=True, choices=('Debug', 'Release'))
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--resources', type=Path, default=hz.ROOT / 'Hazel/Resources')
    parser.add_argument('--mode', choices=('capture', 'timing', 'presented', 'probes', 'relocation'), default='capture')
    parser.add_argument('--effects', action='store_true', help='Enable High AO and TAA for presentation')
    parser.add_argument('--archives', type=Path)
    parser.add_argument('--offscreen-only', action='store_true',
                        help='Verify archive inventories and the relocated Metal probe without launching GUI apps')
    args = parser.parse_args()
    if args.effects and args.mode != 'presented':
        parser.error('--effects requires --mode presented')
    if args.mode == 'relocation' and (args.config != 'Release' or not args.archives):
        parser.error('Relocation requires Release archives produced by hazel.py package')
    if args.offscreen_only and args.mode != 'relocation':
        parser.error('--offscreen-only requires --mode relocation')
    resources, output = args.resources.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        raise RuntimeError('Use a fresh evidence directory')
    project = hz.ROOT / 'examples/RenderingReference/RenderingReference-CombinedShadows.hproj'
    sdk = hz.mono_prefix()
    env = {k: v for k, v in os.environ.items() if not k.startswith(('HAZEL_', 'MTL_', 'DYLD_'))}
    env.update(HAZEL_COMBINED_SHADOW_QUALIFICATION='1', HAZEL_RESOURCES=str(resources),
               HAZEL_RENDERING_REFERENCE_PROJECT=str(project), DYLD_LIBRARY_PATH=str(sdk / 'lib'),
               HAZEL_MONO=str(sdk))
    report = dict(head=hz.output(['git', 'rev-parse', 'HEAD']), configuration=args.config,
                  mode=args.mode, cases={}, shaderSHA256=hashlib.sha256(
                      (resources / 'shaders/ForwardPBR.glsl').read_bytes()).hexdigest(),
                  metalArtifactSHA256=hashlib.sha256(
                      (resources / 'shaders/metal/ForwardPBR.hzmetal').read_bytes()).hexdigest())

    def run(name, executable, *arguments):
        command = list(map(str, (hz.binaries(args.config) / executable / executable, *arguments)))
        log = output / f'{name}.log'
        with log.open('w') as stream:
            proc = subprocess.run(command, env={**env, 'HAZEL_DATA': str(output / f'{name}-profile')},
                                  stdout=stream, stderr=subprocess.STDOUT, timeout=300, cwd=hz.ROOT)
        report['cases'][name] = dict(command=command, exitCode=proc.returncode,
            summary=[line for line in log.read_text(errors='replace').splitlines()
                     if line.startswith(('PASS', 'FAIL', 'POINT', 'SPOT', 'Renderer:', 'METAL_FINAL'))])
        if proc.returncode:
            raise RuntimeError(f'{name} failed, see {log}')

    try:
        if args.mode == 'relocation':
            report['artifactSHA256'] = report['metalArtifactSHA256']
            verify_archives(args.archives.resolve(), output, hz.binaries(args.config), sdk, env, report,
                            mode='point', offscreen_only=args.offscreen_only, probe_env={'HAZEL_COMBINED_SHADOW_QUALIFICATION': '1'})
        elif args.mode == 'presented':
            native_console(output / 'console.json')
            env.update(HAZEL_LOCAL_PRESENT_BENCHMARK='1', HAZEL_METAL_TRACE='1')
            effects = ('2', '--taa') if args.effects else ('-1',)
            run('metal', 'MetalM1Smoke', output / 'metal', '--benchmark-metal', resources, project, *effects)
            report['presentation'] = presented_timings(output / 'metal.log', 'combined')
        elif args.mode == 'probes':
            env.update(MTL_DEBUG_LAYER='1', MTL_SHADER_VALIDATION='1')
            env.pop('HAZEL_COMBINED_SHADOW_QUALIFICATION')
            env['HAZEL_SHADOW_EDGE_PROBES'] = '1'
            report['profiles'] = {}
            for kind in ('Spot', 'Point'):
                env['HAZEL_RENDERING_REFERENCE_PROJECT'] = str(project.with_name(f'RenderingReference-{kind}Shadows.hproj'))
                for backend, binary in (('metal', 'MetalRenderer2DQuadSmoke'), ('gl', 'MigrationRenderer2DSmoke')):
                    name = f'{kind.lower()}-{backend}'
                    folder = output / name
                    flag = f'--{kind.lower()}-shadows'
                    run(name, binary, *((resources, folder, flag) if backend == 'metal' else (flag, folder)))
                    if kind == 'Spot':
                        shaded, clear = (pixels(folder / (capture + '.bin')) for capture in ('hard', 'unshadowed'))
                        profiles = [edge_profile(shaded, clear, row, 135, 169, True) for row in range(95, 136)]
                        report['profiles'][name] = dict(widthPx=stats(width for width, _ in profiles))
                        names = ('hard', 'masked')
                    else:
                        clear = pixels(folder / 'profile-clear.bin')
                        frames = [pixels(folder / f'profile-{step}.bin') for step in range(9)]
                        profiles = [[edge_profile(frame, clear, row, 201, 236) for row in range(80, 99)]
                                    for frame in frames]
                        initial = profiles[0]
                        # The right edge projects from the light-facing face of the
                        # box at z=1.56 onto the z=0 plane. The camera is fixed.
                        advance = .004 * 3 / (3 - 1.56) * 321 / 4.4
                        errors = [abs(position - initial[row][1] - step * advance)
                                  for step, frame in enumerate(profiles) for row, (_, position) in enumerate(frame)]
                        report['profiles'][name] = dict(widthPx=stats(width for frame in profiles for width, _ in frame),
                            edgeMotionErrorPx=stats(errors), maxEdgeMotionErrorPx=max(errors))
                        names = ('profile-0', 'face--1_-1_-1', 'masked', 'contact')
                    for capture in names:
                        image = folder / (capture + '.bin')
                        png(image.with_suffix('.png'), 321, 239, pixels(image))
        elif args.mode == 'timing':
            env['HAZEL_COMBINED_SHADOW_TIMING'] = '1'
            run('metal', 'MetalRenderer2DQuadSmoke', resources, output / 'metal', '--point-shadows')
            report['warmups'], report['samples'] = 5, 15
            report['timing'] = summarize_timings(output / 'metal/combined.csv')
        else:
            env.update(MTL_DEBUG_LAYER='1', MTL_SHADER_VALIDATION='1')
            run('metal', 'MetalRenderer2DQuadSmoke', resources, output / 'metal', '--point-shadows')
            run('opengl', 'MigrationRenderer2DSmoke', '--point-shadows', output / 'opengl')
            report['parity'] = {}
            for image in sorted((output / 'metal').glob('*.bin')):
                gl = output / 'opengl' / image.name
                if gl.exists():
                    report['parity'][image.stem] = parity(image, gl)
                    png(gl.with_suffix('.png'), 960, 540, pixels(gl))
                png(image.with_suffix('.png'), 960, 540, pixels(image))
    finally:
        (output / 'results.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
