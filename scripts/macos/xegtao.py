#!/usr/bin/env python3
"""Focused native XeGTAO geometry/lighting/lifetime checks and capture inspection.

This runner does not declare the complete editor/player milestone qualified.
"""
import argparse
import binascii
import hashlib
import json
import math
import os
from pathlib import Path
import struct
import subprocess
import sys
import zlib
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import hazel as hz


def pixels(path):
    return list(struct.iter_unpack('<4fif', path.read_bytes()))


def png(path, width, height, values):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', binascii.crc32(kind + data) & 0xffffffff)
    def encoded(value):
        value = max(0, min(1, value))
        return round((12.92 * value if value <= .0031308 else 1.055 * value ** (1 / 2.4) - .055) * 255)
    rows = b''.join(b'\0' + bytes(encoded(p[c]) for p in values[y * width:(y + 1) * width] for c in range(3))
                    for y in reversed(range(height)))
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>2I5B', width, height, 8, 2, 0, 0, 0)) +
                     chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))


def ray_box(origin, direction):
    """Independent slab intersection of the qualification box, in world metres."""
    near, far = 0., float('inf')
    for p, d, lo, hi in zip(origin, direction, (-.25, -.25, .05), (.25, .25, .35)):
        if abs(d) < 1e-12:
            if p < lo or p > hi:
                return None
        else:
            a, b = sorted(((lo - p) / d, (hi - p) / d))
            near, far = max(near, a), min(far, b)
    return near if 0 < near <= far else None


def geometric_reference(captures):
    # Predeclared high-preset RMS <= .15 against cosine hemisphere visibility.
    # Radius/falloff are the authored physical values, not shader-tuned 1.457/2.2.
    samples = 16384
    directions = []
    for i in range(samples):
        r = math.sqrt((i + .5) / samples)
        angle = ((i * .6180339887498949) % 1) * 2 * math.pi
        directions.append((r * math.cos(angle), r * math.sin(angle), math.sqrt(1 - r * r)))
    image = pixels(captures / 'perspective-q2.bin')
    rows = []
    for x in (174, 175, 176, 178, 180, 184, 188, 192):
        # Fixed-camera ray / z=.05 plane intersection, independent of GPU depth.
        world_x = ((x + .5) / 320 * 2 - 1) * (4 - .05) * math.tan(math.pi / 8) * (320 / 180)
        world_y = ((90 + .5) / 180 * 2 - 1) * (4 - .05) * math.tan(math.pi / 8)
        expectations = []
        for y in range(89, 92):
            for xx in range(x - 1, x + 2):
                # Use the nearest primary-ray surface, including box coverage at
                # the silhouette; never compare a hidden floor point to a box pixel.
                ray = (((xx + .5) / 320 * 2 - 1) * math.tan(math.pi / 8) * (320 / 180),
                       ((y + .5) / 180 * 2 - 1) * math.tan(math.pi / 8), -1.)
                hit = ray_box((0, 0, 4), ray)
                distance = hit if hit is not None else 4 - .05
                origin = (ray[0] * distance, ray[1] * distance, 4 - distance + 1e-6)
                occlusion = 0.
                for direction in directions:
                    distance = ray_box(origin, direction)
                    if distance is not None:
                        occlusion += max(0., min(1., (.5 - distance) / (.5 * .615)))
                expectations.append(1 - occlusion / samples)
        expected = sum(expectations) / len(expectations)
        measured = sum(image[y * 320 + xx][0] for y in range(89, 92) for xx in range(x - 1, x + 2)) / 9
        rows.append(dict(pixel=[x, 90], world=[world_x, world_y, .05], expected=expected, measured=measured,
                         error=measured - expected))
    rms = math.sqrt(sum(row['error'] ** 2 for row in rows) / len(rows))
    return dict(samplesPerProbe=samples, probes=rows, rms=rms, limit=.15, status='PASS' if rms <= .15 else 'FAIL',
                scope='Independent physical-radius cosine hemisphere visibility; screen-space approximation, high quality only')


def inspect(captures):
    for path in captures.glob('*.bin'):
        values = pixels(path)
        width, height = (640, 360) if path.stem.startswith('demo-') else (320, 180)
        if len(values) != width * height:
            raise RuntimeError('Unexpected capture extent: ' + str(path))
        png(path.with_suffix('.png'), width, height, values)
    reference = geometric_reference(captures)
    (captures / 'geometric-reference.json').write_text(json.dumps(reference, indent=2) + '\n')
    if reference['status'] != 'PASS':
        raise RuntimeError('Independent AO geometric reference failed; inspect geometric-reference.json')
    return reference


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', choices=('Debug', 'Release'), required=True)
    parser.add_argument('--resources', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--inspect-only', action='store_true')
    args = parser.parse_args()
    if args.inspect_only:
        print(json.dumps(inspect(args.output), indent=2))
        return
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        raise RuntimeError('Use a fresh evidence directory')
    env = os.environ.copy()
    env.update(DYLD_LIBRARY_PATH=str(hz.mono_prefix() / 'lib'), MTL_DEBUG_LAYER='1', MTL_SHADER_VALIDATION='1')
    command = [hz.binaries(args.config) / 'MetalRenderer2DQuadSmoke/MetalRenderer2DQuadSmoke',
               args.resources.resolve(), output / 'captures', '--xegtao']
    with (output / 'native.log').open('w') as log:
        result = subprocess.run(list(map(str, command)), cwd=hz.ROOT, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=120)
    report = dict(configuration=args.config, head=hz.output(['git', 'rev-parse', 'HEAD']),
                  worktree=hz.output(['git', 'status', '--short']), command=list(map(str, command)),
                  nativeStatus='PASS' if result.returncode == 0 else 'FAIL',
                  shaderHashes={p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in (args.resources / 'shaders/metal').glob('*') if p.is_file()},
                  milestone='INCOMPLETE: focused offscreen checks exclude real editor/player, display scaling, packaging and sustained benchmarks')
    (output / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    if result.returncode:
        raise RuntimeError('Native XeGTAO checks failed: ' + str(output / 'native.log'))
    report['geometricReference'] = inspect(output / 'captures')
    (output / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    print('PASS focused native geometry, lighting separation, lifetime and independent reference')


if __name__ == '__main__':
    main()
