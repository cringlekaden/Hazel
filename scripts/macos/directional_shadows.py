#!/usr/bin/env python3
"""Directional CSM analytic visibility, deterministic captures and native parity."""
import argparse
import hashlib
import csv
import statistics
import math
import json
import os
from pathlib import Path
import subprocess
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import hazel as hz
from macos.xegtao import pixels,png

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config',choices=('Debug','Release'),required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--resources',type=Path,required=True)
    parser.add_argument('--benchmark',action='store_true')
    parser.add_argument('--backend',choices=('metal','opengl','both'),default='both')
    args=parser.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    if any(out.iterdir()):raise RuntimeError('Use a fresh evidence directory')
    env=os.environ.copy();env.update(DYLD_LIBRARY_PATH=str(hz.mono_prefix()/'lib'),
        HAZEL_RESOURCES=str(args.resources.resolve()),HAZEL_RENDERING_REFERENCE_PROJECT=str(hz.ROOT/'examples/RenderingReference/RenderingReference.hproj'),
        HAZEL_DATA=str(out/'profile'),MTL_DEBUG_LAYER='1',MTL_SHADER_VALIDATION='1')
    if args.benchmark:
        env['HAZEL_SHADOW_BENCHMARK']='1';env.pop('MTL_DEBUG_LAYER',None);env.pop('MTL_SHADER_VALIDATION',None)
    report=dict(sourceSHA256=hashlib.sha256((args.resources/'shaders/ForwardPBR.glsl').read_bytes()).hexdigest(),benchmark=args.benchmark,head=hz.output(['git','rev-parse','HEAD']),configuration=args.config,commands={},backends={})
    try:
        for backend in (('metal','opengl') if args.backend=='both' else (args.backend,)):
            folder=out/backend
            cmd=[hz.binaries(args.config)/'MetalRenderer2DQuadSmoke/MetalRenderer2DQuadSmoke',args.resources.resolve(),folder,'--shadows'] if backend=='metal' else [hz.binaries(args.config)/'MigrationRenderer2DSmoke/MigrationRenderer2DSmoke','--shadows',folder]
            report['commands'][backend]=list(map(str,cmd))
            with (out/(backend+'.log')).open('w') as log:
                result=subprocess.run(list(map(str,cmd)),cwd=hz.ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=600)
            if result.returncode:raise RuntimeError(backend+' failed; inspect '+str(out/(backend+'.log')))
            if backend=='metal' and not args.benchmark:
                temporal=[hz.binaries(args.config)/'MetalRenderer2DQuadSmoke/MetalRenderer2DQuadSmoke',args.resources.resolve(),folder/'temporal','--shadow-temporal-lifetime']
                report['commands']['temporalLifetime']=list(map(str,temporal))
                with (out/'temporal-lifetime.log').open('w') as log:
                    checked=subprocess.run(list(map(str,temporal)),cwd=hz.ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=120)
                if checked.returncode:raise RuntimeError('Shadow + TAA lifetime failed')
            if args.benchmark:
                groups={}
                for row in csv.DictReader((folder/'benchmark.csv').open()):
                    groups.setdefault('/'.join(row[k] for k in ('width','height','resolution','enabled')),[]).append(row)
                summary={}
                for key,rows in groups.items():
                    summary[key]={}
                    for field in ('shadowGPUms','forwardGPUms','serializedFrameMs'):
                        values=sorted(float(r[field]) for r in rows)
                        summary[key][field]=dict(median=statistics.median(values),p95=values[math.ceil(.95*len(values))-1])
                report['backends'][backend]=dict(samples=20,warmup=5,cases=summary,scope='32 cube casters plus ground; serialized offscreen readback. GPU generation/forward counters available only on native Metal; forward-on minus forward-off includes PCF. Not presentation or frame pacing.')
                continue
            captures=sorted(folder.glob('case-*.bin'));report['backends'][backend]=dict(cases=len(captures))
            for file in captures:png(file.with_suffix('.png'),384,256,pixels(file))
            png(folder/'study.png',1280,720,pixels(folder/'study.bin'))
        if args.backend=='both' and not args.benchmark:
            parity=[]
            for file in sorted((out/'metal').glob('case-*.bin')):
                a=pixels(file);b=pixels(out/'opengl'/file.name)
                case=dict(name=file.stem,idMismatches=sum(x[4]!=y[4] for x,y in zip(a,b)),
                    maxDepth=max(abs(x[5]-y[5]) for x,y in zip(a,b)),maxRGB=max(abs(x[c]-y[c]) for x,y in zip(a,b) for c in range(3)))
                parity.append(case)
            report['parity']=parity
            if any(p['idMismatches'] or p['maxDepth']>1e-6 or p['maxRGB']>.003 for p in parity):raise RuntimeError('CSM backend parity failed')
        report['status']='MEASURED scoped native pass timing; not presentation or a performance acceptance gate' if args.benchmark else 'PASS scoped analytic visibility and deterministic captures; full milestone gates separate'
    except Exception as error:
        report['status']='INCOMPLETE';report['error']=str(error);raise
    finally:(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    print(report['status'])
if __name__=='__main__':main()
