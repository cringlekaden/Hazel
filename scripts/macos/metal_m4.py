#!/usr/bin/env python3
"""Capture shared production 3D scenes and independent oracles on native Metal/OpenGL.

This focused offscreen check never declares the complete M4 milestone qualified.
Real-window, extended lifetime and performance gates remain separate.
"""
import argparse
from concurrent.futures import ProcessPoolExecutor
import hashlib
import json
import math
import os
from pathlib import Path
import struct
import subprocess
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import hazel as hz
from macos.metal_m3 import compare


def probes(native, opengl):
    a, b = native.read_bytes(), opengl.read_bytes()
    if len(a) != len(b) or not a or len(a) % 24:
        raise RuntimeError('Invalid 3D readback extent: '+native.name)
    color = depth = relative = 0.0
    ids = 0
    for x, y in zip(struct.iter_unpack('<4fif', a), struct.iter_unpack('<4fif', b)):
        if not all(math.isfinite(v) for v in (*x[:4],x[5],*y[:4],y[5])):
            raise RuntimeError('Nonfinite 3D readback: '+native.name)
        ids += x[4] != y[4]
        depth = max(depth, abs(x[5]-y[5]))
        for u, v in zip(x[:4],y[:4]):
            color = max(color, abs(u-v))
            relative = max(relative, abs(u-v)/max(abs(u),abs(v),.02))
    return dict(pixels=len(a)//24, idMismatches=ids, maxDepthError=depth,
                maxColorError=color, maxFloorRelativeColorError=relative,
                nativeSHA256=hashlib.sha256(a).hexdigest(), openGLSHA256=hashlib.sha256(b).hexdigest(),
                colorStatus='MEASURED: independent per-case oracle gates run inside both executables')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', choices=('Debug','Release'), required=True)
    parser.add_argument('--resources', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--comparison-workers', type=int, choices=(1,2), default=2)
    args = parser.parse_args()
    output=args.output.resolve();output.mkdir(parents=True,exist_ok=True)
    if any(output.iterdir()): raise RuntimeError('Use a fresh evidence directory')
    os.chdir(hz.ROOT)
    env=os.environ.copy()
    env.update(DYLD_LIBRARY_PATH=str(hz.mono_prefix()/'lib'),HAZEL_RESOURCES=str(args.resources.resolve()),
               HAZEL_DATA=str(output/'gl-user'),HAZEL_METAL_TRACE='1',MTL_DEBUG_LAYER='1',MTL_SHADER_VALIDATION='1')
    commands=[('metal',[hz.binaries(args.config)/'MetalRenderer2DQuadSmoke/MetalRenderer2DQuadSmoke',
                         args.resources.resolve(),output/'metal','--renderer3d-parity']),
              ('opengl',[hz.binaries(args.config)/'MigrationRenderer2DSmoke/MigrationRenderer2DSmoke',
                         '--renderer3d-parity',output/'opengl'])]
    for name,command in commands:
        with (output/(name+'.log')).open('w') as log:
            run=subprocess.run(list(map(str,command)),env=env,stdout=log,stderr=subprocess.STDOUT,timeout=300)
        if run.returncode:raise RuntimeError(name+' failed; inspect '+str(output/(name+'.log')))
    with ProcessPoolExecutor(max_workers=args.comparison_workers) as pool:
        futures = {}
        for path in sorted((output/'metal').glob('*.bin')):
            other=output/'opengl'/path.name
            if '-linear-' in path.name or path.stem=='independent-hdr':
                futures[path.stem]=pool.submit(probes,path,other)
            else:futures[path.stem]=pool.submit(compare,path,other,path.stem!='Direct-PerspectiveUnlit')
        results={name:future.result() for name,future in futures.items()}
    if len(results)!=135: raise RuntimeError('Incomplete production fixture inventory')
    result=dict(configuration=args.config,head=hz.output(['git','rev-parse','HEAD']),captures=results,
                independentOracles='PASS in both native executables',
                milestone='INCOMPLETE: this runner does not qualify real windows or extended resource stability')
    (output/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    if any(x.get('verdict')=='FAIL' or x['idMismatches'] or x['maxDepthError']>1e-6 for x in results.values()):
        raise RuntimeError('Production 3D comparison failed; inspect results.json')
    print('PASS focused production captures and independent oracles; full M4 qualification remains separate')


if __name__=='__main__': main()
