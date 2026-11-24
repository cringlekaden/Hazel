#!/usr/bin/env python3
"""Serialized native XeGTAO off/preset measurements through the production view.

Counter scopes are GPU completion intervals, not exclusive/additive pass costs.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import statistics
import subprocess
import sys
import time
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import hazel as hz
from macos.qualification import native_console


def distribution(values):
    values = sorted(values)
    if not values:
        return dict(samples=0)
    return dict(samples=len(values), medianMs=statistics.median(values), p95Ms=values[int(.95 * (len(values) - 1))])


def summarize(text):
    presentation = {int(frame):float(stamp) for frame, stamp in re.findall(r'M1_PRESENT frame=(\d+) timestamp=([\d.]+)', text) if float(stamp) > 0}
    presented = set(presentation)
    joins = {int(cpu): int(gpu) for cpu, gpu in re.findall(r'M4_BENCH_JOIN cpu_frame=(\d+) submission=(\d+)', text)}
    cpu = {int(frame): (extent, float(scene), float(layer)) for frame, extent, scene, layer in
           re.findall(r'M4_BENCH_CPU frame=(\d+) pixels=(\d+x\d+) scene_ms=([\d.]+) layer_ms=([\d.]+)', text)}
    def accepted(frame):
        return 120 < (frame - 720 if frame > 720 else frame) <= 700
    gpu = {int(frame): (float(start), float(end)) for frame, start, end in
           re.findall(r'M1_FEEDBACK frame=(\d+) gpu_start=([\d.]+) gpu_end=([\d.]+)', text)
           if int(frame) in presented and 0 < float(start) <= float(end)}
    phases = {}
    for frame, extent, depth, prefilter, main, denoise, forward, memory in re.findall(
            r'XEGTAO_GPU frame=(\d+) pixels=(\d+x\d+) depth_ms=([\d.]+) prefilter_ms=([\d.]+) main_ms=([\d.]+) denoise_ms=([\d.]+) forward_world_ms=([\d.]+) working_bytes=(\d+)', text):
        phases[int(frame)] = (extent, *map(float, (depth, prefilter, main, denoise, forward)), int(memory))
    result = {}
    for extent in ('1280x720', '1920x1080'):
        frames = [f for f, row in cpu.items() if row[0] == extent and accepted(f)]
        submitted = [joins[f] for f in frames if f in joins]
        row = dict(sceneCPU=distribution([cpu[f][1] for f in frames]), layerCPU=distribution([cpu[f][2] for f in frames]),
                   wholeSubmissionGPU=distribution([(gpu[f][1] - gpu[f][0]) * 1000 for f in submitted if f in gpu]),
                   submitCPU=distribution([float(ms) for f, ex, ms in re.findall(r'M1_SUBMIT frame=(\d+) pixels=(\d+x\d+) cpu_ms=([\d.]+)', text)
                                           if ex == extent and int(f) in submitted]),
                   cpuLoopInterval=distribution([float(ms) for f, ex, ms in re.findall(r'M6_BENCH_PACING frame=(\d+) pixels=(\d+x\d+) ms=([\d.]+)', text)
                                                if ex == extent and int(f) in frames]))
        for index, name in enumerate(('depthPreparation', 'depthPrefilter', 'main', 'denoise', 'forwardWorld'), 1):
            row[name + 'GPUCompletionInterval'] = distribution([phases[f][index] for f in submitted if f in gpu and f in phases and phases[f][0] == extent])
        phase_valid=sum(f in gpu and f in phases and phases[f][0]==extent for f in submitted)
        phase_candidates=sum(f in gpu for f in submitted) if phases else 0
        row['phaseCounterSamples']={'positivelyPresentedCandidates':phase_candidates,'validOrderedRecords':phase_valid,'rejectedOrMissingRecords':phase_candidates-phase_valid}
        row['AOTextureBytesPerGeneration'] = max((phases[f][-1] for f in submitted if f in phases), default=0)
        stamps = sorted(gpu[f][1] for f in submitted if f in gpu)
        row['GPUCompletionSpacing'] = distribution([(b - a) * 1000 for a, b in zip(stamps, stamps[1:])])
        if row['wholeSubmissionGPU']['samples'] < 300:
            raise RuntimeError('Too few positively presented GPU samples: ' + extent)
        stamps = sorted(presentation[f] for f in submitted if f in presentation)
        row['positivePresentationSpacing'] = distribution([(b-a)*1000 for a,b in zip(stamps,stamps[1:])])
        result[extent] = row
    final = re.findall(r'METAL_FINAL submitted=(\d+) completed=(\d+) presented=(\d+) completed_presented=(\d+) errors=(\d+) peak_frame_bytes=(\d+) peak_attachment_bytes=(\d+) peak_allocator_bytes=(\d+) live_attachment_generations=(\d+) pending_bytes=(\d+)', text)
    if not final:
        raise RuntimeError('Native retirement evidence missing')
    values = list(map(int, final[-1]))
    if values[0] != values[1] or min(values[2:4]) < 600 or values[4] or values[8] or values[9]:
        raise RuntimeError('Native completion/presentation/retirement gate failed')
    reuse=[dict(zip(('frame','frameBytes','attachmentBytes','generations','attachmentAllocations','attachmentReuses','bufferAllocations','bufferReuses','allocatorBytes','inflight'),map(int,row))) for row in re.findall(r'M3_RESOURCES frame=(\d+) frame_bytes=(\d+) attachment_bytes=(\d+) generations=(\d+) attachment_allocations=(\d+) attachment_reuses=(\d+) buffer_allocations=(\d+) buffer_reuses=(\d+) allocator_bytes=(\d+) inflight=(\d+)',text)]
    return dict(extents=result, resourceSamples=reuse, final=dict(zip(('submitted','completed','presented','completedPresented','errors','peakFrameBytes','peakAttachmentBytes','peakAllocatorBytes','liveAttachmentGenerations','pendingBytes'), values)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', choices=('Debug', 'Release'), required=True)
    parser.add_argument('--resources', type=Path, required=True)
    parser.add_argument('--project', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve(); output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        raise RuntimeError('Use a fresh evidence directory')
    native_console(output / 'console.json')
    env = os.environ.copy()
    for name in ('MTL_DEBUG_LAYER','MTL_SHADER_VALIDATION','MTL_CAPTURE_ENABLED'):
        env.pop(name, None)
    env.update(DYLD_LIBRARY_PATH=str(hz.mono_prefix() / 'lib'), HAZEL_METAL_TRACE='1')
    asset_root=args.project.parent/'Assets'
    inputs=[args.project,asset_root/'Scenes/XeGTAO-Contact.hazel',asset_root/'Studies/XeGTAO/DarkMetal.hmat',
            asset_root/'Reference/IBL_v1/Photo.henv',asset_root/'Reference/IBL_v1/Photo.henv.accepted']
    inputs += [p for folder in (asset_root/'Native/Direct_v1',asset_root/'Reference/IBL_v1/Photo.henv.prepared',args.resources/'shaders/metal') for p in folder.rglob('*') if p.is_file()]
    report = dict(configuration=args.config, head=hz.output(['git', 'rev-parse', 'HEAD']),
                  worktree=hz.output(['git', 'status', '--short']), warmup=120, tail=20, framesPerExtent=720,
                  scopes='Scene CPU, native submission CPU, tick interval, whole commit feedback, and precise GPU phase completion intervals. Phase intervals include synchronization and are not exclusive/additive. No backend speedup claim.',
                  memory='AO texture bytes are per generation; tracked frame/attachment/allocator peaks overlap and must not be added. RSS includes CPU/driver process storage.',
                  inputs={str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},
                  worktreeDiffSHA256=hashlib.sha256(subprocess.check_output(['git','diff','HEAD'],cwd=hz.ROOT)).hexdigest(),runs={})
    try:
        for quality, name in enumerate(('off','low','medium','high','ultra'), -1):
            folder = output / name; folder.mkdir()
            command = list(map(str, [hz.binaries(args.config) / 'MetalM1Smoke/MetalM1Smoke', folder,
                                    '--benchmark-metal', args.resources.resolve(), args.project.resolve(), quality]))
            start = time.monotonic(); rss = []
            with (folder / 'run.log').open('w') as log:
                process = subprocess.Popen(command, cwd=hz.ROOT, env=env, stdout=log, stderr=subprocess.STDOUT)
                while process.poll() is None:
                    if time.monotonic() - start > 120:
                        process.kill(); process.wait(); raise RuntimeError('Benchmark timed out: ' + name)
                    sample = subprocess.run(['/bin/ps','-o','rss=','-p',str(process.pid)], capture_output=True, text=True)
                    if sample.stdout.strip():
                        rss.append(int(sample.stdout.strip()))
                    time.sleep(.1)
            if process.returncode:
                raise RuntimeError('Benchmark failed: ' + str(folder / 'run.log'))
            report['runs'][name] = dict(command=command, seconds=time.monotonic() - start, peakRSSKiB=max(rss, default=0),
                                        **summarize((folder / 'run.log').read_text()))
            (output / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
            print('PASS measured', name, flush=True)
        report['status'] = 'PASS: measured matrix; complete feature qualification remains separate'
    finally:
        (output / 'results.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
