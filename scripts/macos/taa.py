#!/usr/bin/env python3
"""Focused native TAA motion/identity/lifetime qualification; no visual acceptance claim."""
import argparse
import hashlib
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
    parser.add_argument('--fast-edges',action='store_true',help='Normal-speed oscillating camera, .01 near / 1000 far (requires --edges)')
    parser.add_argument('--sloped-edges',action='store_true',help='Hold out steep 60-degree depth-gradient silhouettes (requires --edges)')
    parser.add_argument('--edges',action='store_true',help='AO-free analytic silhouette convergence and motion diagnostics')
    parser.add_argument('--config',choices=('Debug','Release'),required=True)
    parser.add_argument('--resources',type=Path,default=hz.ROOT/'Hazel/Resources')
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();
    if args.fast_edges and not args.edges:parser.error('--fast-edges requires --edges')
    if args.sloped_edges and not args.edges:parser.error('--sloped-edges requires --edges')
    output=args.output.resolve();output.mkdir(parents=True,exist_ok=True)
    if any(output.iterdir()):raise RuntimeError('Use fresh evidence directory')
    env=os.environ.copy();env.update(DYLD_LIBRARY_PATH=str(hz.mono_prefix()/'lib'),MTL_DEBUG_LAYER='1',MTL_SHADER_VALIDATION='1')
    if args.sloped_edges:env['HAZEL_TAA_EDGE_SLOPED']='1'
    else:env.pop('HAZEL_TAA_EDGE_SLOPED',None)
    if args.fast_edges:env['HAZEL_TAA_EDGE_FAST']='1'
    else:env.pop('HAZEL_TAA_EDGE_FAST',None)
    command=[hz.binaries(args.config)/'MetalRenderer2DQuadSmoke/MetalRenderer2DQuadSmoke',args.resources.resolve(),output/'captures','--taa-edges' if args.edges else '--taa']
    with (output/'native.log').open('w') as log:
        result=subprocess.run(list(map(str,command)),cwd=hz.ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
    report=dict(configuration=args.config,head=hz.output(['git','rev-parse','HEAD']),worktree=hz.output(['git','status','--short']),
        command=list(map(str,command)),status='PASS' if result.returncode==0 else 'FAIL',
        scope=('AO-free analytic silhouettes, projection jitter and serialized/three-slot equivalence' if args.edges else 'Offscreen rigid camera/object motion, exact IDs/depth, cancellation, views and resize')+'. Owner visual acceptance pending.',
        shaders={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (args.resources/'shaders/metal').glob('*') if p.is_file()})
    if args.edges and result.returncode==0:
        from macos.taa_edges import inspect
        report['edgeQuality']=inspect(output/'captures/edges.csv')
        report['edgeGeometry']='steeply sloped' if args.sloped_edges else 'front-facing'
        report['edgeCameraMotion']='normal-speed oscillation, small near / long far' if args.fast_edges else 'slow constant motion'
        if report['edgeQuality']['status']!='PASS':report['status']='FAIL'
    (output/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    for path in (output/'captures').glob('*.bin'):
        values=pixels(path);png(path.with_suffix('.png'),320,180,values)
    if report['status']!='PASS':raise RuntimeError('TAA fixture failed: '+str(output/'native.log'))
    print('PASS focused native TAA; visual acceptance INCOMPLETE')
if __name__=='__main__':main()
