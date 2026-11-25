#!/usr/bin/env python3
"""Multiscattering_v2 native component, metal/color sweep and moving-highlight study."""
import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import subprocess
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import hazel as hz
from macos.xegtao import pixels,png
BASELINE='3b517f86ac3a5955136a2f26a2dc0e056f3550ac'

def baseline_resources(output):
    root=output/'single-resources';root.mkdir()
    for child in (hz.ROOT/'Hazel/Resources').iterdir():
        if child.name in ('shaders','BRDF'):shutil.copytree(child,root/child.name)
        else:(root/child.name).symlink_to(child,target_is_directory=child.is_dir())
    for name in ('shaders/ForwardPBR.glsl','shaders/metal/ForwardPBR.hzmetal'):
        (root/name).write_bytes(subprocess.check_output(['git','show',BASELINE+':Hazel/Resources/'+name],cwd=hz.ROOT))
    # Qualification-only old data/old shader pair; no production legacy branch.
    shutil.copyfile(root/'BRDF/GGX-correlated-AB.hlight',root/'BRDF/GGX-MS_v2.hlight')
    (root/'BRDF/GGX-MS_v2.json').write_text(json.dumps(dict(qualificationOnly=True,recipe='Historical IBL_v1',source='Immutable accepted 128-square A/B; historical shader sampler convention'))+'\n')
    return root

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--config',choices=('Debug','Release'),required=True)
    p.add_argument('--project',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--single',action='store_true',help='Test-only historical shader/expectations, same current executable/resources')
    args=p.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    if any(out.iterdir()):raise RuntimeError('Use fresh evidence directory')
    resources=baseline_resources(out) if args.single else hz.ROOT/'Hazel/Resources'
    env=os.environ.copy();env.update(DYLD_LIBRARY_PATH=str(hz.mono_prefix()/'lib'),
        HAZEL_RESOURCES=str(resources),HAZEL_DATA=str(out/'profile'),
        HAZEL_RENDERING_REFERENCE_PROJECT=str(args.project.resolve()),HAZEL_METAL_TRACE='1',MTL_DEBUG_LAYER='1',MTL_SHADER_VALIDATION='1')
    env.pop('HAZEL_GGX_SINGLE_REFERENCE',None)
    if args.single:env['HAZEL_GGX_SINGLE_REFERENCE']='1'
    report=dict(family='Multiscattering_v2',head=hz.output(['git','rev-parse','HEAD']),singleScattering=args.single,
                configuration=args.config,commands={},modelErrors={},sourceSHA256=hashlib.sha256((resources/'shaders/ForwardPBR.glsl').read_bytes()).hexdigest())
    for backend in ('metal','opengl'):
        folder=out/backend
        command=[hz.binaries(args.config)/'MetalRenderer2DQuadSmoke/MetalRenderer2DQuadSmoke',resources,folder,'--ggx-ms'] if backend=='metal' else [hz.binaries(args.config)/'MigrationRenderer2DSmoke/MigrationRenderer2DSmoke','--ggx-ms',folder]
        report['commands'][backend]=list(map(str,command))
        with (out/(backend+'.log')).open('w') as log:
            run=subprocess.run(list(map(str,command)),cwd=hz.ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=300)
        if run.returncode:raise RuntimeError('GGX '+backend+' failed: '+str(out/(backend+'.log')))
        rows=list(csv.DictReader((folder/'components.csv').open()))
        if len(rows)!=735:raise RuntimeError('Incomplete component inventory')
        report.setdefault('continuousModelErrors',{})[backend]=max(abs(float(row[c])-float(row['continuous'+c.upper()])) for row in rows for c in 'rgb')
        if not args.single and report['continuousModelErrors'][backend]>.002:
            raise RuntimeError('Current GGX exceeds the .002 independent continuous-model bound: '+backend)
        report['modelErrors'][backend]=max(abs(float(row[c])-float(row['expected'+c.upper()])) for row in rows for c in 'rgb')
        for file in folder.glob('*.bin'):
            values=pixels(file)
            if len(values)!=640*360 or not all(math.isfinite(v) for row in values for v in (*row[:4],row[5])):
                raise RuntimeError('Nonfinite/incomplete FP32 study capture')
            png(file.with_suffix('.png'),640,360,values)
    a=list(csv.DictReader((out/'metal/components.csv').open()));b=list(csv.DictReader((out/'opengl/components.csv').open()))
    report['componentBackendMax']=max(abs(float(x[c])-float(y[c])) for x,y in zip(a,b) for c in 'rgb')
    report['images']={}
    for file in (out/'metal').glob('*.bin'):
        x,y=pixels(file),pixels(out/'opengl'/file.name)
        ids=sum(a[4]!=b[4] for a,b in zip(x,y));depth=max(abs(a[5]-b[5]) for a,b in zip(x,y))
        report['images'][file.stem]=dict(idMismatches=ids,maxDepthError=depth,maxRGB=max(abs(a[c]-b[c]) for a,b in zip(x,y) for c in range(3)))
        if ids or depth>1e-6 or report['images'][file.stem]['maxRGB']>.002:raise RuntimeError('GGX image backend parity failed')
    if report['componentBackendMax']>.002:raise RuntimeError('GGX component backend parity failed')
    report['status']='PASS native numerical/components/parity/retirement; owner visual acceptance pending'
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(report['status'])
if __name__=='__main__':main()
