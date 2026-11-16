#!/usr/bin/env python3
"""Compare shared Renderer2D and authored world/UI GPU readbacks on native Metal/OpenGL.

This offscreen gate does not qualify visible presentation or physical UI input.
"""
import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import re
import statistics
import struct
import subprocess
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import hazel as hz


def compare(native, opengl, half_display=False):
    a, b = native.read_bytes(), opengl.read_bytes()
    if len(a) != len(b) or len(a) % 24:
        raise RuntimeError('Readback length mismatch: ' + native.name)
    color = depth = 0.0
    ids = 0
    precision_failures = 0
    max_display_delta = 0
    for x, y in zip(struct.iter_unpack('<4fif', a), struct.iter_unpack('<4fif', b)):
        if not all(math.isfinite(v) for v in (*x[:4],x[5],*y[:4],y[5])):
            raise RuntimeError('Nonfinite GPU readback: '+native.name)
        ids += x[4] != y[4]
        color = max(color, *(abs(u-v) for u, v in zip(x[:4], y[:4])))
        depth = max(depth, abs(x[5]-y[5]))
        if half_display:
            for u,v in zip(x[:4],y[:4]):
                # World conversion + panel + glyph: at most three half-float
                # attachment stores, plus the strict FP32 shader bound.
                ulp = max(2**-24, 2**(math.floor(math.log2(max(abs(u),abs(v))))-10)) if u or v else 2**-24
                precision_failures += abs(u-v) > 3*ulp + 2e-5
            def encoded(value):
                value=max(0,min(1,value))
                return round(255*(12.92*value if value<=.0031308 else 1.055*value**(1/2.4)-.055))
            max_display_delta=max(max_display_delta,*(abs(encoded(u)-encoded(v)) for u,v in zip(x[:3],y[:3])))
    result = dict(pixels=len(a)//24, idMismatches=ids, maxColorError=color, maxDepthError=depth,
                  nativeSHA256=hashlib.sha256(a).hexdigest(), openGLSHA256=hashlib.sha256(b).hexdigest())
    color_pass = precision_failures == 0 and max_display_delta <= 1 if half_display else color <= 2e-5
    if half_display: result.update(halfFloatBoundFailures=precision_failures,maxEncoded8BitDelta=max_display_delta,
        colorBound='3 FP16 ULP per channel plus 2e-5 FP32 shader bound; encoded RGB <= 1 level')
    result['verdict'] = 'PASS' if ids == 0 and color_pass and depth <= 1e-6 else 'FAIL'
    return result


def distribution(values):
    values = sorted(values)
    return dict(samples=len(values), medianMs=statistics.median(values),
                p95Ms=values[int(.95*(len(values)-1))]) if values else None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', choices=('Debug', 'Release'), default='Debug')
    parser.add_argument('--resources', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--build', action='store_true')
    parser.add_argument('--texture-slots',type=int,choices=(2,4),default=4)
    args = parser.parse_args()
    os.chdir(hz.ROOT)
    output = args.output.resolve(); output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        raise RuntimeError('Use a fresh evidence directory')
    if args.build:
        hz.build(args.config, tests=True, targets=['MetalRenderer2DQuadSmoke', 'MigrationRenderer2DSmoke'])
    env = os.environ.copy()
    env.update(HAZEL_METAL_TRACE='1', HAZEL_RESOURCES=str(args.resources.resolve()),
               HAZEL_DATA=str(output/'gl-profile'), DYLD_LIBRARY_PATH=str(hz.mono_prefix()/'lib'))
    commands = [
        [sys.executable, hz.ROOT/'scripts/macos/metal_m2b2.py', '--config', args.config,
         '--resources', args.resources.resolve(), '--output', output/'metal', '--texture-slots', str(args.texture_slots)],
        [hz.binaries(args.config)/'MigrationRenderer2DSmoke/MigrationRenderer2DSmoke',
         '--metal-parity', output/'opengl']]
    for name, command in zip(('metal', 'opengl'), commands):
        with (output/(name+'.log')).open('w') as log:
            run = subprocess.run(list(map(str, command)), cwd=hz.ROOT, env=env,
                                 stdout=log, stderr=subprocess.STDOUT, timeout=240)
        if run.returncode:
            raise RuntimeError(name+' gate failed; see '+str(output/(name+'.log')))
    measurements = {}
    for phase in range(4):
        measurements['mixed-'+str(phase)] = compare(output/'metal'/f'mixed-metal-{phase}.bin',
                                                   output/'opengl'/f'mixed-opengl-{phase}.bin')
    for extent in ('1280x720', '1920x1080'):
        measurements['runtime-'+extent] = compare(output/'metal'/f'runtime-{extent}.bin',
                                                  output/'opengl'/f'runtime-{extent}.bin', half_display=True)
    with (output/'metal/runtime-cpu.csv').open() as file:
        samples = list(csv.DictReader(file))
    log = (output/'metal/native.log').read_text()
    gpu = {int(f):(float(end)-float(start))*1000 for f, start, end in
           re.findall(r'M3_OFFSCREEN_FEEDBACK frame=(\d+) gpu_start=([\d.]+) gpu_end=([\d.]+)', log)
           if float(start)>0 and float(end)>=float(start)}
    timings = {}
    # Runtime scene submissions follow the 25 preserved/basic mixed fixtures.
    for width in ('1280', '1920'):
        rows = [(i, r) for i, r in enumerate(samples) if r['width']==width and 10 <= int(r['frame']) < 70]
        timings[width] = dict(warmupFrames=10, tailExcluded=10,
            sceneCPU=distribution([float(r['runtime_ms']) for _,r in rows]),
            submitCPU=distribution([float(r['submit_ms']) for _,r in rows]),
            gpu=distribution([gpu[26+i] for i,_ in rows if 26+i in gpu]),
            drawCounts=sorted({int(r['draws']) for _,r in rows}),
            batchCounts=sorted({int(r['batches']) for _,r in rows}),
            geometryCounts=sorted({int(r['geometry']) for _,r in rows}),
            attachmentBytes=sorted({int(r['attachment_bytes']) for _,r in rows}),
            attachmentAllocations=sorted({int(r['attachment_allocations']) for _,r in rows}),
            attachmentReuses=[int(rows[0][1]['attachment_reuses']), int(rows[-1][1]['attachment_reuses'])],
            bufferAllocations=sorted({int(r['buffer_allocations']) for _,r in rows}),
            bufferReuses=[int(rows[0][1]['buffer_reuses']),int(rows[-1][1]['buffer_reuses'])],
            retainedBufferBytes=sorted({int(r['retained_buffer_bytes']) for _,r in rows}))
    result = dict(verdict='PASS' if all(m['verdict']=='PASS' for m in measurements.values()) else 'FAIL',
        scope='Offscreen native Apple GPU only; full M3 acceptance remains separate', configuration=args.config,
        commands=[list(map(str,c)) for c in commands], measurements=measurements, timings=timings,
        timingScope='Validation-enabled Scene/RuntimeUI CPU, Submit CPU, GPU world/Neutral/UI/readback; no presentation or device polling',
        colorTolerance=2e-5, depthTolerance=1e-6, signedIDs='exact',
        validation='Metal API and GPU shader validation enabled by metal_m2b2.py',
        nativeMetrics=json.loads((output/'metal/metrics.json').read_text()))
    (output/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
    if result['verdict'] != 'PASS':
        raise RuntimeError('Native Renderer2D parity differs; preserve raw images and exact ID masks')


if __name__ == '__main__':
    main()
