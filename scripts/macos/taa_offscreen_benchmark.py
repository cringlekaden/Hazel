#!/usr/bin/env python3
"""Matched completed offscreen TAA/AO matrix; excludes presentation and frame pacing.

Readback plus synchronous capture are intentional. These scopes cannot replace
or be compared as application speedups against the presented XeGTAO baseline.
"""
import argparse
import csv
import json
import os
from pathlib import Path
import re
import subprocess
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import hazel as hz
from macos.xegtao_benchmark import distribution

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config',choices=('Debug','Release'),default='Release')
    parser.add_argument('--resources',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();output=args.output.resolve();output.mkdir(parents=True,exist_ok=True)
    if any(output.iterdir()):raise RuntimeError('Use fresh evidence directory')
    env=os.environ.copy()
    for key in ('MTL_DEBUG_LAYER','MTL_SHADER_VALIDATION','MTL_CAPTURE_ENABLED'):env.pop(key,None)
    env.update(DYLD_LIBRARY_PATH=str(hz.mono_prefix()/'lib'),HAZEL_METAL_TRACE='1')
    command=[hz.binaries(args.config)/'MetalRenderer2DQuadSmoke/MetalRenderer2DQuadSmoke',args.resources.resolve(),output/'raw','--taa-offscreen-benchmark']
    with (output/'native.log').open('w') as log:
        result=subprocess.run(list(map(str,command)),cwd=hz.ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=600)
    report=dict(head=hz.output(['git','rev-parse','HEAD']),worktree=hz.output(['git','status','--short']),
        command=list(map(str,command)),configuration=args.config,scope=__doc__,warmup=40,samplesPerCombination=120,
        presentation='NOT RUN: offscreen completion only; desktop-dependent timing/lifecycle separate',runs=[])
    (output/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    if result.returncode:raise RuntimeError('Offscreen matrix failed: '+str(output/'native.log'))
    text=(output/'native.log').read_text()
    gpu={int(f):(float(end)-float(start))*1000 for f,start,end in re.findall(r'M3_OFFSCREEN_FEEDBACK frame=(\d+) gpu_start=([\d.]+) gpu_end=([\d.]+)',text) if 0<float(start)<float(end)}
    rows=list(csv.DictReader((output/'raw/offscreen-timings.csv').open()))
    for width,height in ((1280,720),(1920,1080)):
        for quality in range(-1,4):
            for taa in (0,1):
                selected=[r for r in rows if int(r['width'])==width and int(r['height'])==height and int(r['quality'])==quality and int(r['taa'])==taa and int(r['iteration'])>=40]
                if len(selected)!=120:raise RuntimeError('Incomplete matched matrix')
                row=dict(width=width,height=height,quality=quality,taa=bool(taa))
                for key in ('scene_ms','capture_wait_ms','frame_ms','depth_ms','prefilter_ms','main_ms','denoise_ms','forward_world_ms','resolve_ms','shading_ms','motion_ms'):
                    row[key]=distribution([float(r[key]) for r in selected])
                row['completedCommitGPU']=distribution([gpu[int(r['serial'])] for r in selected if int(r['serial']) in gpu])
                if row['completedCommitGPU']['samples']<100:raise RuntimeError('Missing completed GPU feedback')
                for key in ('working_bytes','history_bytes','history_allocations','history_reuses'):row[key]=max(int(r[key]) for r in selected)
                report['runs'].append(row)
    final=re.findall(r'METAL_FINAL submitted=(\d+) completed=(\d+) presented=(\d+) completed_presented=(\d+) errors=(\d+) peak_frame_bytes=(\d+) peak_attachment_bytes=(\d+) peak_allocator_bytes=(\d+) live_attachment_generations=(\d+) pending_bytes=(\d+)',text)
    if not final:raise RuntimeError('Missing retirement record')
    values=list(map(int,final[-1]));report['final']=dict(zip(('submitted','completed','presented','completedPresented','errors','peakFrameBytes','peakAttachmentBytes','peakAllocatorBytes','liveAttachmentGenerations','pendingBytes'),values))
    if values[0]!=values[1] or values[4] or values[8] or values[9]:raise RuntimeError('Offscreen retirement failed')
    report['status']='PASS completed offscreen measurements; presentation qualification INCOMPLETE'
    (output/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(report['status'])
if __name__=='__main__':main()
