#!/usr/bin/env python3
"""Native local-light FP64/culling parity or matched serialized GPU benchmarks."""
import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import statistics
import subprocess
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import hazel as hz
from macos.xegtao import pixels,png

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--config',choices=('Debug','Release'),required=True)
    p.add_argument('--output',type=Path,required=True)
    mode=p.add_mutually_exclusive_group()
    mode.add_argument('--benchmark',action='store_true')
    mode.add_argument('--smooth-terminator',action='store_true',help='Require the analytic smooth-sphere horizon oracle')
    mode.add_argument('--terminator',action='store_true',help='Require the geometric-horizon diagnostic')
    p.add_argument('--resources',type=Path,default=hz.ROOT/'Hazel/Resources')
    p.add_argument('--benchmark-focus',action='store_true',help='Only 1080p tiled-16: 32/128 sparse and 320 dense (requires --benchmark)')
    p.add_argument('--backend',choices=('metal','opengl','both'),default='both')
    args=p.parse_args()
    if args.benchmark_focus and not args.benchmark:p.error('--benchmark-focus requires --benchmark')
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    if any(out.iterdir()):raise RuntimeError('Use a fresh evidence directory')
    env=os.environ.copy();env.update(DYLD_LIBRARY_PATH=str(hz.mono_prefix()/'lib'),
        HAZEL_RESOURCES=str(args.resources.resolve()),HAZEL_RENDERING_REFERENCE_PROJECT=str(hz.ROOT/'examples/RenderingReference/RenderingReference.hproj'),
        HAZEL_DATA=str(out/'profile'),MTL_DEBUG_LAYER='1',MTL_SHADER_VALIDATION='1')
    if args.benchmark:env['HAZEL_LOCAL_BENCHMARK']='terminator' if args.benchmark_focus else '1';env.pop('MTL_SHADER_VALIDATION',None);env.pop('MTL_DEBUG_LAYER',None)
    smooth_terminator=not args.benchmark and not args.terminator
    if smooth_terminator:env['HAZEL_SMOOTH_TERMINATOR_DIAGNOSTIC']='1'
    if args.terminator:env['HAZEL_LOCAL_TERMINATOR_DIAGNOSTIC']='1'
    report=dict(smoothTerminator=smooth_terminator,terminator=args.terminator,head=hz.output(['git','rev-parse','HEAD']),configuration=args.config,benchmark=args.benchmark,
        sourceSHA256=hashlib.sha256((args.resources/'shaders/ForwardPBR.glsl').read_bytes()).hexdigest(),commands={},backends={})
    try:
        for backend in (('metal','opengl') if args.backend=='both' else (args.backend,)):
            folder=out/backend
            cmd=[hz.binaries(args.config)/'MetalRenderer2DQuadSmoke/MetalRenderer2DQuadSmoke',args.resources.resolve(),folder,'--local-lights'] if backend=='metal' else [hz.binaries(args.config)/'MigrationRenderer2DSmoke/MigrationRenderer2DSmoke','--local-lights',folder]
            report['commands'][backend]=list(map(str,cmd))
            with (out/(backend+'.log')).open('w') as log:result=subprocess.run(list(map(str,cmd)),cwd=hz.ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=600)
            if result.returncode:raise RuntimeError(backend+' failed; inspect '+str(out/(backend+'.log')))
            if args.benchmark:
                groups={}
                for row in csv.DictReader((folder/'benchmark.csv').open()):
                    key='/'.join(row[c] for c in ('width','height','count','sparse','culling','tileSize'))
                    groups.setdefault(key,[]).append(row)
                summary={}
                for key,rows in groups.items():
                    summary[key]={}
                    for field in ('collectionCPUms','encodeCPUms','lightPrepCPUms','serializedFrameMs','cullGPUms','forwardGPUms','lightBytes','tileBytes','candidates','overflowTiles'):
                        values=sorted(float(r[field]) for r in rows)
                        summary[key][field]=dict(median=statistics.median(values),p95=values[math.ceil(.95*len(values))-1])
                report['backends'][backend]=dict(samplesPerCase=20,warmup=5,cases=summary,
                    scope='Alternating point/spot lights, one contributing central point in sparse scenes. Serialized offscreen render + completion + readback. Not presented frame pacing; GPU counters bound culling and forward/world execution. CPU light preparation overlaps recording and includes deferred native resources; intervals are not additive.')
            else:
                rows=list(csv.DictReader((folder/'local-lights.csv').open()))
                if len(rows)!=40:raise RuntimeError('Incomplete numerical inventory')
                report['backends'][backend]=dict(cases=len(rows),maximumFP64Error=max(float(r['maximumError']) for r in rows),
                    maximumOrderAndCullingError=max(float(r['orderError']) for r in rows),maximumOverflowTiles=max(int(r['overflowTiles']) for r in rows))
                image=pixels(folder/'showcase.bin');png(folder/'showcase.png',1280,720,image)
        if not args.benchmark and args.backend=='both':
            a=pixels(out/'metal/showcase.bin');b=pixels(out/'opengl/showcase.bin')
            report['showcaseParity']=dict(idMismatches=sum(x[4]!=y[4] for x,y in zip(a,b)),maxDepth=max(abs(x[5]-y[5]) for x,y in zip(a,b)),
                maxRGB=max(abs(x[c]-y[c]) for x,y in zip(a,b) for c in range(3)))
            if report['showcaseParity']['idMismatches'] or report['showcaseParity']['maxDepth']>1e-6 or report['showcaseParity']['maxRGB']>.003:
                raise RuntimeError('Showcase backend parity failed')
        report['status']='PASS scoped benchmark' if args.benchmark else 'PASS scoped numerical/culling/parity; full milestone gates are separate'
    except Exception as error:
        report['status']='INCOMPLETE';report['error']=str(error);raise
    finally:(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    print(report['status'])
if __name__=='__main__':main()
