#!/usr/bin/env python3
"""Matched native Metal local-light pacing: 120 positive presentations per extent."""
import argparse
import json
import math
import os
from pathlib import Path
import re
import shutil
import statistics
import subprocess
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import hazel as hz
from macos.qualification import native_console

def entity(identity,order,name,position,component):
    return f'''  - Entity: {identity}
    Relationship:
      Parent: 0
      Order: {order}
    TagComponent:
      Tag: {name}
    TransformComponent:
      Translation: {position}
      Rotation: [0, 0, 0]
      Scale: [1, 1, 1]
{component}'''

def scene(count,sparse):
    result='''SceneVersion: 3
Scene: Local-light native benchmark
Presentation:
  Mode: SceneReferred
  Exposure: 0
  Background: [0.01, 0.01, 0.01, 1]
Entities:
'''
    result+=entity(1,0,'Camera',[0,0,3],'''    CameraComponent:
      Camera:
        ProjectionType: 1
        PerspectiveFOV: 0.785398185
        PerspectiveNear: 0.1
        PerspectiveFar: 10
        OrthographicSize: 1.2
        OrthographicNear: 0.1
        OrthographicFar: 10
      Primary: true
      FixedAspectRatio: false
''')
    result+=entity(2,1,'Neutral rough dielectric',[0,0,0],'''    MeshRendererComponent:
      Version: 1
      Mesh: Native/Direct_v1/UnitCube.hmesh
      Materials: []
''')
    for i in range(count):
        spacing=.6 if sparse else .04
        position=[(i%17-8)*spacing,(i%13-6)*spacing,1.5] if i else [0,0,1.5]
        result+=entity(100+i,2+i,'Local light '+str(i),position,f'''    PointLightComponent:
      Color: [1, 1, 1]
      Intensity: 0.25
      Enabled: true
      Range: {1.5 if sparse else 4}
''')
    return result

def stats(values):
    values=sorted(values)
    if len(values)<30:raise RuntimeError('Insufficient matched positive presentation samples')
    return dict(samples=len(values),median=statistics.median(values),p95=values[math.ceil(.95*len(values))-1])

def presented_timings(log,name):
    text=log.read_text();cpu={};joins={};gpu={};present={}
    for m in re.finditer(r'M4_BENCH_CPU frame=(\d+) pixels=(\d+)x(\d+) scene_ms=([\d.]+) layer_ms=([\d.]+)',text):cpu[int(m[1])]=(m[2]+'x'+m[3],float(m[4]),float(m[5]))
    for m in re.finditer(r'M4_BENCH_JOIN cpu_frame=(\d+) submission=(\d+)',text):joins[int(m[2])]=int(m[1])
    for m in re.finditer(r'M1_FEEDBACK frame=(\d+) gpu_start=([\d.]+) gpu_end=([\d.]+)',text):gpu[int(m[1])]=(float(m[3])-float(m[2]))*1000
    for m in re.finditer(r'M1_PRESENT frame=(\d+) timestamp=([\d.]+)',text):
        if float(m[2])>0:present[int(m[1])]=float(m[2])
    groups={}
    for serial,time in present.items():
        row=cpu.get(joins.get(serial));duration=gpu.get(serial)
        if row and duration is not None:groups.setdefault(row[0],[]).append((time,duration,row[1],row[2]))
    case={}
    for extent,rows in groups.items():
        rows=sorted(rows)[30:]
        if len(rows)<60:raise RuntimeError('Insufficient completed positive frames for '+name+' '+extent)
        case[extent]=dict(positiveFrames=len(rows),gpuWholeFrameMs=stats([r[1] for r in rows]),
            sceneCPUms=stats([r[2] for r in rows]),layerCPUms=stats([r[3] for r in rows]),
            presentedIntervalMs=stats([(b[0]-a[0])*1000 for a,b in zip(rows,rows[1:])]))
    if set(case)!= {'1280x720','1920x1080'}:raise RuntimeError('Physical extents differ: '+str(case.keys()))
    return case

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True)
    args=p.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    if any(out.iterdir()):raise RuntimeError('Use a fresh evidence directory')
    native_console(out/'console.json');source=hz.ROOT/'examples/RenderingReference'
    project=out/'project';project.mkdir();shutil.copytree(source/'Assets',project/'Assets')
    descriptor=project/'Benchmark.hproj';descriptor.write_text((source/'RenderingReference.hproj').read_text().replace('Scenes/Core_v1-Orthographic.hazel','Scenes/LocalBenchmark.hazel'))
    env=os.environ.copy();env.update(DYLD_LIBRARY_PATH=str(hz.mono_prefix()/'lib'),HAZEL_LOCAL_PRESENT_BENCHMARK='1',HAZEL_METAL_TRACE='1')
    for name in ('MTL_DEBUG_LAYER','MTL_SHADER_VALIDATION'):env.pop(name,None)
    report=dict(head=hz.output(['git','rev-parse','HEAD']),scope='Native VSync-off request; actual positive presentations only; 30 warmup presentations per physical extent; AO/TAA off.',cases={})
    try:
        for count in (0,1,8,32,128,256,320):
            for sparse in (False,True):
                name=f'{count}-'+('sparse' if sparse else 'dense');folder=out/name
                (project/'Assets/Scenes/LocalBenchmark.hazel').write_text(scene(count,sparse))
                env.update(HAZEL_DATA=str(folder/'profile'),HAZEL_SAVE_ROOT=str(folder/'saves'))
                cmd=[hz.binaries('Release')/'MetalM1Smoke/MetalM1Smoke',folder,'--benchmark-metal',hz.binaries('Release')/'Nutella/Resources',descriptor,'-1']
                with (out/(name+'.log')).open('w') as log:r=subprocess.run(list(map(str,cmd)),env=env,stdout=log,stderr=subprocess.STDOUT,timeout=120)
                if r.returncode:raise RuntimeError('Native pacing failed: '+name)
                case=presented_timings(out/(name+'.log'),name)
                report['cases'][name]=case;print('PASS',name,flush=True)
        report['status']='PASS matched native positive-presentation pacing'
    except Exception as error:report['status']='INCOMPLETE';report['error']=str(error);raise
    finally:(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
