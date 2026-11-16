#!/usr/bin/env python3
"""Build/run the visible M1 native Metal 4 proof with private output and API validation."""
import argparse
from contextlib import contextmanager
import ctypes as ct
import json
import os
import plistlib
import re
import statistics
from pathlib import Path
import subprocess
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import hazel as hz

@contextmanager
def native_hidpi(output):
    """Temporarily use an advertised 2x mode; restore the exact original mode."""
    cg=ct.CDLL('/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics')
    cf=ct.CDLL('/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation')
    def bind(lib,name,args,result):
        fn=getattr(lib,name); fn.argtypes=args; fn.restype=result; return fn
    ptr=ct.c_void_p; uint=ct.c_uint32
    main=bind(cg,'CGMainDisplayID',[],uint)
    copy=bind(cg,'CGDisplayCopyDisplayMode',[uint],ptr)
    modes_for=bind(cg,'CGDisplayCopyAllDisplayModes',[uint,ptr],ptr)
    set_mode=bind(cg,'CGDisplaySetDisplayMode',[uint,ptr,ptr],ct.c_int)
    count=bind(cf,'CFArrayGetCount',[ptr],ct.c_long)
    at=bind(cf,'CFArrayGetValueAtIndex',[ptr,ct.c_long],ptr)
    release=bind(cf,'CFRelease',[ptr],None)
    create=bind(cf,'CFDictionaryCreateMutable',[ptr,ct.c_long,ptr,ptr],ptr)
    put=bind(cf,'CFDictionarySetValue',[ptr,ptr,ptr],None)
    getters={key:bind(cg,'CGDisplayModeGet'+name,[ptr],kind) for key,name,kind in (
        ('width','Width',ct.c_size_t),('height','Height',ct.c_size_t),
        ('pixelWidth','PixelWidth',ct.c_size_t),('pixelHeight','PixelHeight',ct.c_size_t),
        ('id','IODisplayModeID',uint),('refreshHz','RefreshRate',ct.c_double))}
    def describe(mode): return {key:fn(mode) for key,fn in getters.items()}
    display=main(); original=copy(display)
    if not original: raise RuntimeError('No active display mode for native 2x qualification')
    options=modes=None
    record={'display':display,'before':describe(original),'restored':False}
    def save(): (output/'display-mode.json').write_text(json.dumps(record,indent=2)+'\n')
    try:
        options=create(None,0,ct.addressof((ct.c_byte*1).in_dll(cf,'kCFTypeDictionaryKeyCallBacks')),
                       ct.addressof((ct.c_byte*1).in_dll(cf,'kCFTypeDictionaryValueCallBacks')))
        put(options,ptr.in_dll(cg,'kCGDisplayShowDuplicateLowResolutionModes').value,
            ptr.in_dll(cf,'kCFBooleanTrue').value)
        modes=modes_for(display,options)
        if not modes: raise RuntimeError('Cannot enumerate native display modes')
        candidates=[]
        for i in range(count(modes)):
            mode=at(modes,i); info=describe(mode)
            if (info['width'],info['height'],info['pixelWidth'],info['pixelHeight'])==(1280,720,2560,1440):
                candidates.append((info['refreshHz'],mode,info))
        if not candidates: raise RuntimeError('Display exposes no native 1280x720 logical / 2560x1440 physical 2x mode')
        _,mode,info=max(candidates,key=lambda entry:entry[0]); record['selected']=info; save()
        error=set_mode(display,mode,None)
        if error: raise RuntimeError('CGDisplaySetDisplayMode failed: '+str(error))
        current=copy(display)
        try: record['active']=describe(current)
        finally: release(current)
        save()
        yield
    finally:
        error=set_mode(display,original,None)
        current=copy(display)
        try: record['after']=describe(current)
        finally: release(current)
        record['restoreError']=error; record['restored']=error==0 and record['after']==record['before']; save()
        if modes: release(modes)
        if options: release(options)
        release(original)
        if not record['restored']: raise RuntimeError('Original display mode was not restored; see display-mode.json')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config',choices=('Debug','Release'),required=True)
    parser.add_argument('--build',action='store_true')
    parser.add_argument('--quads', type=Path, help='Run the existing integrated quad scene with this Metal shader resource root')
    parser.add_argument('--renderer2d', type=Path, help='Run the production mixed world and screen UI example')
    parser.add_argument('--capture',action='store_true')
    parser.add_argument('--hidpi',action='store_true',help='Temporarily select and verify an advertised native 2x display mode, then restore it')
    parser.add_argument('--build-only',action='store_true',help='Build without requiring an unlocked desktop')
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if hz.SYSTEM!='macos': raise RuntimeError('Metal M1 requires native macOS')
    output=args.output.resolve(); output.mkdir(parents=True,exist_ok=True)
    with hz.build_lock():
        if args.build or args.build_only: hz.build(args.config,tests=True,targets=['MetalM1Smoke'])
        if args.build_only: return
        registry=plistlib.loads(subprocess.check_output(['ioreg','-a','-l','-d1']))
        if isinstance(registry,list): registry=registry[0]
        console={key:registry.get(key) for key in ('IOConsoleLocked','IOConsoleUsers')}
        (output/'console-before.json').write_text(json.dumps(console,indent=2)+'\n')
        sessions=console.get('IOConsoleUsers') or []
        if console['IOConsoleLocked'] is not False or not any(s.get('kCGSSessionOnConsoleKey') and s.get('kCGSessionLoginDoneKey') and s.get('kCGSSessionUserIDKey')==os.getuid() and not s.get('CGSSessionScreenIsLocked') for s in sessions):
            raise RuntimeError('Stop native qualification: intended GUI console is locked or unavailable')
        environment=os.environ.copy(); environment['DYLD_LIBRARY_PATH']=str(hz.mono_prefix()/'lib'); environment.update(MTL_DEBUG_LAYER='1',MTL_DEBUG_LAYER_ERROR_MODE='assert',MTL_DEBUG_LAYER_WARNING_MODE='nslog',MTL_SHADER_VALIDATION='1',MTL_CAPTURE_ENABLED='1')
        command=[hz.binaries(args.config)/'MetalM1Smoke/MetalM1Smoke',output]
        if (args.quads or args.renderer2d) and args.capture: raise RuntimeError('Use either --quads or --capture')
        if args.quads: command += ['--quads', args.quads.resolve()]; environment['HAZEL_METAL_TRACE']='1'
        if args.renderer2d: command += ['--renderer2d', args.renderer2d.resolve()]; environment['HAZEL_METAL_TRACE']='1'
        if args.capture: command.append(output/'metal4.gputrace')
        # Capture both output streams, preserving the full native validation diagnostics.
        def run():
            with (output/'run.log').open('w') as log:
                return subprocess.run(list(map(str,command)),cwd=hz.ROOT,env=environment,stdout=log,stderr=subprocess.STDOUT,timeout=120)
        if args.hidpi:
            with native_hidpi(output):
                result=run()
                evidence=json.loads((output/'environment.json').read_text())
                if evidence['cocoaBackingScale']!=2 or evidence['glfwScaleX']!=2 or evidence['glfwScaleY']!=2:
                    raise RuntimeError('Actual smoke process did not observe native 2x scaling')
                if (evidence['framebufferWidth'],evidence['framebufferHeight'])!=(2*evidence['logicalWidth'],2*evidence['logicalHeight']):
                    raise RuntimeError('Native 2x GLFW framebuffer extent mismatch')
        else: result=run()
        print((output/'run.log').read_text(),end='')
        if result.returncode: raise RuntimeError('Metal M1 failed with status '+str(result.returncode))
        log=(output/'run.log').read_text()
        if 'Validation Error' in log or 'failed assertion' in log or 'FAIL:' in log:
            raise RuntimeError('Metal validation/test errors in run.log')
        # Join positive presentation timestamps to completed GPU work. The original
        # transfer mode also verifies readback; scene numerical checks run separately.
        gpu={int(frame) for frame,ok in re.findall(r'M1_GPU frame=(\d+) verified=(\d+)',log) if ok=='1'}
        if args.quads or args.renderer2d: gpu={int(frame) for frame in re.findall(r'M2C_GPU frame=(\d+) completed=1',log)}
        presented={int(frame):float(stamp) for frame,stamp in re.findall(r'M1_PRESENT frame=(\d+) timestamp=([\d.]+)',log) if float(stamp)>0}
        qualified=sorted(gpu & presented.keys())
        timings={int(frame):(int(w),int(h),float(ms)) for frame,w,h,ms in re.findall(r'M1_SUBMIT frame=(\d+) pixels=(\d+)x(\d+) cpu_ms=([\d.]+)',log)}
        gpu_times={int(frame):(float(end)-float(start))*1000 for frame,start,end in re.findall(r'M1_FEEDBACK frame=(\d+) gpu_start=([\d.]+) gpu_end=([\d.]+)',log) if float(start)>0 and float(end)>=float(start)}
        baseline=[frame for frame,entry in timings.items() if entry[:2]==(1280,720)]
        proof={'completedVerified':len(gpu),'positivePresented':len(presented),'completedVerifiedPositivePresented':len(qualified),
               'frames':[{'frame':frame,'presentedTime':presented[frame]} for frame in qualified],
               'timingScope':('Validation-enabled integrated scene, world/output conversion and presentation; numeric fixtures qualified separately.' if (args.quads or args.renderer2d) else 'Validation-enabled clear + diagnostic transfers. Not scene-rendering performance.'),
               'baseline1280x720':{'samples':len(baseline),
                   'cpuMeanMs':statistics.mean(timings[f][2] for f in baseline) if baseline else None,
                   'gpuMeanMs':statistics.mean(gpu_times[f] for f in baseline if f in gpu_times) if any(f in gpu_times for f in baseline) else None}}
        if args.quads or args.renderer2d:
            proof['completed']=proof.pop('completedVerified')
            proof['completedPositivePresented']=proof.pop('completedVerifiedPositivePresented')
            proof['verification']='GPU completion and positive presentation; numerical fixture results are separate'
            upper=max(gpu_times)-10
            steady=[f for f in gpu_times if 130<=f<=upper and f in timings]
            def distribution(values):
                values=sorted(values)
                return {'samples':len(values),'medianMs':statistics.median(values),'p95Ms':values[int(.95*(len(values)-1))]} if values else None
            proof['steadySceneTiming']={'warmup':'first 130 submissions and final 10 excluded',
                'cpuScope':'Present polling, drawable acquisition, immutable GPU preparation, encoding and commit; excludes Scene/RuntimeSession CPU work',
                'dimensions':sorted(set((timings[f][0],timings[f][1]) for f in steady)),
                'cpu':distribution([timings[f][2] for f in steady]),'gpu':distribution([gpu_times[f] for f in steady])}
        (output/'frame-proof.json').write_text(json.dumps(proof,indent=2)+'\n')
        if len(qualified)<300: raise RuntimeError('Fewer than 300 independently correlated completed/verified/positive-presented frames')
if __name__=='__main__': main()
