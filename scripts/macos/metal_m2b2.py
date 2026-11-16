#!/usr/bin/env python3
"""Qualify the bounded production Renderer2D offscreen quad; no GUI required."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import hazel as hz


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', choices=['Debug', 'Release'], default='Debug')
    parser.add_argument('--build', action='store_true')
    parser.add_argument('--resources', type=Path, required=True, help='Resource root containing prebuilt shaders/metal/*.hzmetal')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    os.chdir(hz.ROOT)
    output = args.output.resolve(); output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        raise RuntimeError('Use a fresh evidence directory')
    if args.build:
        hz.build(args.config, tests=True, targets=['MetalRenderer2DQuadSmoke'])
    env = os.environ.copy()
    env.update(PATH='/nonexistent', DEVELOPER_DIR='/nonexistent', DYLD_LIBRARY_PATH=str(hz.mono_prefix() / 'lib'),
               MTL_DEBUG_LAYER='1', MTL_DEBUG_LAYER_ERROR_MODE='assert', MTL_SHADER_VALIDATION='1')
    command = [hz.binaries(args.config) / 'MetalRenderer2DQuadSmoke/MetalRenderer2DQuadSmoke', args.resources.resolve(), output]
    with (output / 'native.log').open('w') as log:
        run = subprocess.run(list(map(str, command)), stdout=log, stderr=subprocess.STDOUT, env=env, timeout=120)
    text = (output / 'native.log').read_text()
    if run.returncode or re.search(r'Validation Error|failed assertion|GPU fault|FAIL:', text):
        raise RuntimeError('Native quad qualification failed: ' + str(output / 'native.log'))
    metrics = json.loads((output / 'metrics.json').read_text())
    files = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in output.glob('*.bin')}
    if files['nearest-camera-versions.bin'] != files['nearest-after-reuse.bin']:
        raise RuntimeError('Retained readback changed after slot reuse')
    a = [(1,0,0,1), (0,1,0,1), (0,0,1,1), (1,1,1,1)]
    b = [(0,1,1,1), (1,0,1,1), (1,1,0,1), (0,0,0,1)]
    measurements = {}
    for fixture in ('nearest-camera-versions', 'linear', 'repeat'):
        data = (output / (fixture + '.bin')).read_bytes()
        max_color = max_depth = 0.0; samples = 0
        for y in range(32):
            for x in range(32):
                if fixture == 'nearest-camera-versions':
                    if 4 <= y < 28 and (2 <= x < 14 or 18 <= x < 30):
                        left = x < 14; col = int((x - (2 if left else 18)) >= 6); row = int(y < 16)
                        tint = (2,.5,.25,1) if left else (.5,1,2,1)
                        color = [u*v for u,v in zip((a if left else b)[row*2+col], tint)]
                        entity, depth = (-123456789 if left else 123456789), .25
                    else:
                        color, entity, depth = (.125,.25,.5,1), -1, 1
                elif 8 <= x < 24 and 8 <= y < 24:
                    u, v = (x+.5-8)/16, 1-(y+.5-8)/16
                    if fixture == 'linear':
                        s, t = min(1,max(0,u*2-.5)), min(1,max(0,v*2-.5))
                        color = [(a[0][c]*(1-s)+a[1][c]*s)*(1-t)+(a[2][c]*(1-s)+a[3][c]*s)*t for c in range(4)]
                        entity = -987654321
                    else:
                        u, v = (u*2)%1, (v*2)%1
                        color, entity = a[int(v>=.5)*2+int(u>=.5)], -135791113
                    depth = .5
                else:
                    color, entity, depth = (0,0,0,0), -1, 1
                values = struct.unpack_from('<4fif', data, (y*32+x)*24)
                if values[4] != entity:
                    raise RuntimeError(f'{fixture} signed ID mismatch at {x},{y}')
                max_color = max(max_color,max(abs(c-v) for c,v in zip(color,values[:4])))
                max_depth = max(max_depth,abs(depth-values[5])); samples += 1
        if max_color > 1e-6 or max_depth > 1e-6:
            raise RuntimeError(f'{fixture} numeric tolerance exceeded')
        measurements[fixture] = dict(samples=samples,maxColorError=max_color,maxDepthError=max_depth,signedIDs='exact')
    result = dict(verdict='PASS',configuration=args.config,metrics=metrics,measurements=measurements,sha256=files,
                  command=list(map(str,command)),environment={k:env[k] for k in ('PATH','DEVELOPER_DIR','MTL_DEBUG_LAYER','MTL_SHADER_VALIDATION')},
                  artifacts={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (args.resources/'shaders/metal').glob('*.hzmetal')},
                  presentation='NOT RUN: offscreen only')
    (output/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))


if __name__ == '__main__':
    main()
