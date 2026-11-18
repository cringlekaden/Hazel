#!/usr/bin/env python3
"""Matched production-scene window timings using the existing native lifecycle executable."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import plistlib
import re
import statistics
import subprocess
import sys
import time
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import hazel as hz


def distribution(values):
    values=sorted(values)
    if len(values)<500:raise RuntimeError('Fewer than 500 warmed timing samples')
    return dict(samples=len(values),medianMs=statistics.median(values),p95Ms=values[int(.95*(len(values)-1))])


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config',choices=('Debug','Release'),required=True)
    parser.add_argument('--resources',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--matched-formats',action='store_true',help='Diagnostic FP32 display targets on both backends; ordinary application formats unchanged')
    args=parser.parse_args()
    output=args.output.resolve();output.mkdir(parents=True,exist_ok=True)
    if any(output.iterdir()):raise RuntimeError('Use a fresh evidence directory')
    os.chdir(hz.ROOT)
    registry=plistlib.loads(subprocess.check_output(['ioreg','-a','-l','-d1']))
    if isinstance(registry,list):registry=registry[0]
    console={k:registry.get(k) for k in ('IOConsoleLocked','IOConsoleUsers')}
    (output/'console.json').write_text(json.dumps(console,indent=2))
    if console['IOConsoleLocked'] is not False or not any(s.get('kCGSSessionOnConsoleKey') and s.get('kCGSessionLoginDoneKey') and s.get('kCGSSessionUserIDKey')==os.getuid() and not s.get('CGSSessionScreenIsLocked') for s in console.get('IOConsoleUsers',[])):
        raise RuntimeError('Intended GUI console must remain unlocked')
    env=os.environ.copy()
    for key in ('MTL_DEBUG_LAYER','MTL_SHADER_VALIDATION','MTL_CAPTURE_ENABLED'):env.pop(key,None)
    env.update(DYLD_LIBRARY_PATH=str(hz.mono_prefix()/'lib'),HAZEL_METAL_TRACE='1')
    result={'configuration':args.config,'scene':'RenderingReference-IBLQuality-Photo.hproj',
        'settings':'Authored camera/assets, four texture slots, VSync-off request, one sample, no GPU validation instrumentation; same production HDR/display formats as normal Nutella',
        'scope':'GPU intervals cover clear/world/Neutral/UI/output commands; Metal commit feedback may include drawable synchronization. CPU scene/layer excludes presentation; native submit is reported separately. No speedup claim.',
        'pacingBoundary':'CPU background-tick to background-tick, including layer work, output submission, window update and event polling. Not physical display cadence. Startup is constructor + scene load to before Run; not first presentation.',
        'startupCachePolicy':'New application-private profile per backend; OS filesystem and driver caches are uncontrolled. One startup observation per process.',
        'inputSHA256':{str(p.relative_to(hz.ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted((hz.ROOT/'examples/RenderingReference').rglob('*')) if p.is_file() and 'Intermediates' not in p.parts and 'Binaries' not in p.parts},
        'backends':{}}
    result['displayFormat'] = 'RGBA32F both backends (diagnostic)' if args.matched_formats else 'RGBA16F OpenGL / RGBA32F Metal (production)'
    result['depthFormat'] = 'OpenGL Depth24Stencil8 / Metal Depth32F; GPU timing boundaries still differ'
    for backend in ('opengl','metal'):
        folder=output/backend;folder.mkdir()
        command=[hz.binaries(args.config)/'MetalM1Smoke/MetalM1Smoke',folder,'--benchmark-'+backend+('-fp32' if args.matched_formats and backend=='opengl' else ''),args.resources.resolve()]
        (folder/'command.json').write_text(json.dumps(list(map(str,command)),indent=2))
        rss=[]
        with (folder/'run.log').open('w') as log:
            process=subprocess.Popen(list(map(str,command)),env=env,stdout=log,stderr=subprocess.STDOUT)
            start=time.monotonic()
            while process.poll() is None:
                sample=subprocess.run(['ps','-p',str(process.pid),'-o','rss='],capture_output=True,text=True)
                if sample.stdout.strip():rss.append({'seconds':time.monotonic()-start,'rssKiB':int(sample.stdout.strip())})
                if time.monotonic()-start>120:process.terminate();raise RuntimeError('Window benchmark timed out')
                time.sleep(.2)
        (folder/'rss.json').write_text(json.dumps(rss,indent=2))
        text=(folder/'run.log').read_text()
        if process.returncode or 'PASS production window benchmark' not in text:raise RuntimeError('Benchmark failed: '+str(folder/'run.log'))
        cpu={int(f):(int(w),int(h),float(s),float(l),int(d)) for f,w,h,s,l,d in re.findall(r'M4_BENCH_CPU frame=(\d+) pixels=(\d+)x(\d+) scene_ms=([\d.]+) layer_ms=([\d.]+) draws=(\d+)',text)}
        gpu={int(f):float(ms) for f,ms in re.findall(r'M4_BENCH_GPU frame=(\d+) ms=([\d.]+)',text)}
        submit={}
        presentations={}
        if backend=='metal':
            joins={int(s):int(f) for f,s in re.findall(r'M4_BENCH_JOIN cpu_frame=(\d+) submission=(\d+)',text)}
            positive={int(f) for f,t in re.findall(r'M1_PRESENT frame=(\d+) timestamp=([\d.]+)',text) if float(t)>0}
            presentations={joins[int(f)]:float(t) for f,t in re.findall(r'M1_PRESENT frame=(\d+) timestamp=([\d.]+)',text) if int(f) in joins and float(t)>0}
            gpu={joins[int(f)]:(float(b)-float(a))*1000 for f,a,b in re.findall(r'M1_FEEDBACK frame=(\d+) gpu_start=([\d.]+) gpu_end=([\d.]+)',text) if int(f) in joins and int(f) in positive and float(a)>0 and float(b)>=float(a)}
            submit={joins[int(f)]:float(ms) for f,w,h,ms in re.findall(r'M1_SUBMIT frame=(\d+) pixels=(\d+)x(\d+) cpu_ms=([\d.]+)',text) if int(f) in joins}
        pacing={int(f):float(ms) for f,w,h,ms in re.findall(r'M6_BENCH_PACING frame=(\d+) pixels=(\d+)x(\d+) ms=([\d.]+)',text)}
        rows={}
        for w,h in ((1280,720),(1920,1080)):
            frames=sorted(f for f,v in cpu.items() if v[:2]==(w,h))[120:-20]
            gpu_frames=[f for f in frames if f in gpu]
            rows[f'{w}x{h}']={'warmup':120,'tailExcluded':20,'sceneCPU':distribution([cpu[f][2] for f in frames]),'layerCPU':distribution([cpu[f][3] for f in frames]),'gpu':distribution([gpu[f] for f in gpu_frames]),'draws':sorted(set(cpu[f][4] for f in frames))}
            rows[f'{w}x{h}']['loopPacingCPU']=distribution([pacing[f] for f in frames if f in pacing])
            times=sorted(presentations[f] for f in frames if f in presentations)
            if times:rows[f'{w}x{h}']['positivePresentationPacing']=distribution([(b-a)*1000 for a,b in zip(times,times[1:])])
            if submit:rows[f'{w}x{h}']['nativeSubmitCPU']=distribution([submit[f] for f in frames if f in submit])
        memory = [{k:int(v) for k,v in re.findall(r'(frame_bytes|attachment_bytes|allocator_bytes|inflight)=(\d+)', line)} for line in text.splitlines() if line.startswith('M3_RESOURCES ')]
        peaks = {k:max(row[k] for row in memory) for k in memory[0]} if memory else {}
        result['backends'][backend]={'metalResourcePeaks':peaks,'resourceAccounting':'Metal counters overlap; OpenGL driver allocations unavailable. RSS is sampled process resident memory, not GPU memory.', 'extents':rows,'rssPeakKiB':max(x['rssKiB'] for x in rss),'rssSamples':len(rss), 'startupToSceneReadyMs':float(re.search(r'M6_BENCH_STARTUP ms=([\d.]+)',text)[1])}
    (output/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    print('PASS matched production window timing observations; timings do not establish a speedup')


if __name__=='__main__':main()
