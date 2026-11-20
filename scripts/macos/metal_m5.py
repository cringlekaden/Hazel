#!/usr/bin/env python3
"""Run focused M5 production editor-consumer/compute/authored-editor gates.

This does not replace manual editor workflows, physical DPI/multi-monitor checks,
M3/M4 comparisons or the maintained OpenGL suite. A focused PASS is not M5 PASS.
"""
from contextlib import nullcontext
import argparse
import json
import os
from pathlib import Path
import re
import statistics
import shutil
import subprocess
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import hazel as hz


def distribution(values):
    values = sorted(values)
    if not values:
        return {'samples': 0}
    return {'samples': len(values), 'medianMs': statistics.median(values),
            'p95Ms': values[int(.95 * (len(values) - 1))]}


def timings(text):
    presentations = {int(f): float(t) for f, t in re.findall(r'M5_EDITOR_PRESENT frame=(\d+) timestamp=([\d.]+)', text) if float(t) > 0}
    gpu = {int(f): (float(a), float(b)) for f, a, b in re.findall(r'M5_EDITOR_GPU frame=(\d+) start=([\d.]+) end=([\d.]+)', text)
           if 120 < int(f) < 870 and int(f) in presentations and float(a) > 0 and float(b) >= float(a)}
    result = {'wholeSubmissionGPU': distribution([(b-a)*1000 for a, b in gpu.values()])}
    scene, ui = [], []
    rejected = 0
    for frame, frequency, start, rendered, drawn in re.findall(r'M5_EDITOR_GPU_PHASE frame=(\d+) frequency=(\d+) start=(\d+) scene_complete=(\d+) imgui_complete=(\d+)', text):
        frame, frequency, start, rendered, drawn = map(int, (frame, frequency, start, rendered, drawn))
        if not (120 < frame < 870 and frame in presentations): continue
        if not frequency or not (0 < start <= rendered <= drawn):
            rejected += 1
            continue
        scene.append((rendered-start)*1000/frequency)
        ui.append((drawn-rendered)*1000/frequency)
    result['sceneGPUCompletionInterval'] = distribution(scene)
    result['imguiGPUCompletionInterval'] = distribution(ui)
    result['counterRejectedSamples'] = rejected
    lag = [(presentations[f]-b)*1000 for f, (_, b) in gpu.items() if presentations[f] >= b]
    result['gpuEndToPresentation'] = distribution(lag)
    result['presentationLagRejectedSamples'] = len(gpu)-len(lag)
    for key in ('cpu', 'acquire', 'prepare', 'render_encode', 'imgui_encode', 'submit_present', 'queue_commit', 'present_request', 'retire'):
        result[key] = distribution([float(v) for f, v in re.findall(r'M5_EDITOR_SUBMIT frame=(\d+)[^\n]*? ' + key + r'_ms=([\d.]+)', text) if 120 < int(f) < 870])
    for key in ('render', 'imgui'):
        result['application_' + key] = distribution([float(v) for f, v in re.findall(r'M5_EDITOR_CPU frame=(\d+)[^\n]*? ' + key + r'_ms=([\d.]+)', text) if 120 < int(f) < 880])
    return result


def native_result(text, minimum=50, imgui=True):
    matches = re.findall(r'METAL_FINAL submitted=(\d+) completed=(\d+) presented=(\d+) completed_presented=(\d+) errors=(\d+)[^\n]*live_attachment_generations=(\d+) pending_bytes=(\d+)', text)
    if not matches: raise RuntimeError('Native retirement evidence missing')
    submitted, completed, presented, completed_presented, errors, generations, pending = map(int, matches[-1])
    if submitted < minimum or completed != submitted or presented < minimum or completed_presented < minimum or errors or generations or pending or (imgui and 'M5_IMGUI_FINAL bytes=0 ' not in text):
        raise RuntimeError('Native completion/presentation/retirement gate failed')
    return {'status': 'PASS', 'submitted': submitted, 'completed': completed, 'positivePresentationsAtSnapshot': presented, 'completedPositivePresentationsAtSnapshot': completed_presented}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', choices=('Debug', 'Release'), required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--build', action='store_true')
    parser.add_argument('--only', choices=('compute', 'consumers', 'editor', 'workflow', 'shutdown', 'abort', 'recovery'))
    parser.add_argument('--native-2x', action='store_true', help='Temporarily select the existing advertised native 2x display mode and restore it')
    parser.add_argument('--timing', action='store_true', help='Disable GPU validation; these observations are not validation results')
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        raise RuntimeError('Use a fresh evidence directory')
    os.chdir(hz.ROOT)
    if args.build:
        hz.build(args.config, tests=True, targets=['Hazelnut', 'MetalM1Smoke', 'MetalRenderer2DQuadSmoke', 'MigrationEditorSmoke'])
    env = os.environ.copy()
    for key in ('HAZEL_RESOURCES', 'MTL_DEBUG_LAYER', 'MTL_SHADER_VALIDATION', 'MTL_CAPTURE_ENABLED'):
        env.pop(key, None)
    env.update(DYLD_LIBRARY_PATH=str(hz.mono_prefix()/'lib'), HAZEL_METAL_TRACE='1')
    if not args.timing:
        env.update(MTL_DEBUG_LAYER='1', MTL_SHADER_VALIDATION='1')
    report = {'configuration': args.config, 'validationEnabled': not args.timing,
              'milestone': 'INCOMPLETE: focused gates only', 'gates': {},
              'timingScope': 'CPU acquisition, packet preparation, render encoding, ImGui encoding and submit/present are separate. GPU is the entire submission, positively presented samples only. Consumer drawables allow readback from first presentation. GPU counter completion intervals can include later overlapping work; presentation lag uses host timestamps. No exclusive pass-cost or speedup claim.'}

    def run(command, folder, marker, native=False):
        folder.mkdir(parents=True, exist_ok=True)
        report['gates'][folder.name] = {'status': 'FAIL'}
        command = list(map(str, command))
        (folder/'command.json').write_text(json.dumps(command, indent=2))
        local = env.copy()
        local.update(HAZEL_DATA=str(folder/'profile'), HAZEL_SAVE_ROOT=str(folder/'saves'))
        with (folder/'run.log').open('w') as log:
            result = subprocess.run(command, env=local, stdout=log, stderr=subprocess.STDOUT, timeout=1000 if folder.name=='workflow' else 180)
        text = (folder/'run.log').read_text()
        if result.returncode or marker not in text:
            raise RuntimeError('Gate failed: ' + str(folder/'run.log'))
        if native:
            report['gates'][folder.name] = native_result(text)
        else:
            report['gates'][folder.name] = {'status': 'PASS'}
        return text

    from macos.metal_m1 import native_hidpi
    scale = native_hidpi(output) if args.native_2x else nullcontext()
    try:
        with scale:
            binaries = hz.binaries(args.config)
            if args.only in (None, 'compute'):
                folder = output/'compute'
                folder.mkdir()
                source = hz.ROOT/'tests/fixtures/metal/ColorDependency.metal'
                commands = [['xcrun', '-sdk', 'macosx', 'metal', '-std=metal3.0', '-fno-fast-math', '-c', str(source), '-o', str(folder/'ColorDependency.air')],
                            ['xcrun', '-sdk', 'macosx', 'metallib', str(folder/'ColorDependency.air'), '-o', str(folder/'ColorDependency.metallib')]]
                (folder/'producer-commands.json').write_text(json.dumps(commands, indent=2))
                with (folder/'producer.log').open('w') as log:
                    for command in commands:
                        subprocess.run(command, check=True, stdout=log, stderr=subprocess.STDOUT)
                run([binaries/'MetalRenderer2DQuadSmoke/MetalRenderer2DQuadSmoke', hz.ROOT/'Hazel/Resources', folder, '--compute'], folder, 'PASS production render-compute-readback')
            if args.only in (None, 'consumers'):
                text = run([binaries/'MetalM1Smoke/MetalM1Smoke', output/'consumers', '--editor-consumers', binaries/'Hazelnut/Resources'], output/'consumers', 'PASS production editor consumers', native=True)
                per_host = {}
                for _, host, time in re.findall(r'M5_EDITOR_HOST_PRESENT frame=(\d+) viewport=(\d+) timestamp=([\d.]+)', text):
                    if float(time) > 0: per_host[host] = per_host.get(host, 0) + 1
                if len(per_host) != 4 or min(per_host.values()) < 50:
                    report['gates']['consumers']['status'] = 'FAIL'
                    raise RuntimeError('Every native consumer requires at least 50 positive presentations')
                environment = json.loads((output/'consumers/environment.json').read_text())
                backing_scale = environment['cocoaBackingScale']
                if args.native_2x and (backing_scale != 2 or environment['framebufferWidth'] != 2*environment['logicalWidth'] or environment['framebufferHeight'] != 2*environment['logicalHeight']):
                    report['gates']['consumers']['status'] = 'FAIL'
                    raise RuntimeError('Native 2x framebuffer dimensions were not observed')
                report['gates']['consumers']['nativeBackingScale'] = backing_scale
                report['gates']['consumers']['perHostPositivePresentations'] = per_host
                report['timingObservations'] = timings(text)
            if args.only in (None, 'shutdown'):
                text = run([binaries/'MetalM1Smoke/MetalM1Smoke', output/'shutdown', '--editor-pending-shutdown', binaries/'Hazelnut/Resources'], output/'shutdown', 'PASS native editor pending shutdown')
                report['gates']['shutdown']['status'] = 'FAIL'
                result = native_result(text, 1)
                if 'UI_PENDING_SHUTDOWN pending=3' not in text:
                    raise RuntimeError('Pending shutdown did not close with three genuine GPU submissions pending')
                report['gates']['shutdown'] = result
            if args.only in (None, 'abort'):
                run([binaries/'MetalM1Smoke/MetalM1Smoke', output/'abort', '--editor-encode-abort', binaries/'Hazelnut/Resources'], output/'abort', 'PASS native editor UI encoder abort')
            if args.only in (None, 'recovery'):
                run([binaries/'MetalM1Smoke/MetalM1Smoke', output/'recovery', '--editor-encode-recovery', binaries/'Hazelnut/Resources'], output/'recovery', 'PASS native editor UI encoder recovery', native=True)
            if args.only in (None, 'workflow'):
                env['HAZEL_EDITOR_CAPTURE'] = str(output/'workflow/editor.ppm')
                saved = env.get('HAZEL_TEST_METAL_EDITOR')
                env['HAZEL_TEST_METAL_EDITOR'] = '1'
                try:
                    run([sys.executable, hz.ROOT/'scripts/hazel.py', 'test', '--config', args.config, '--only', 'EditorSmoke'], output/'workflow', '1 sequential regression executables')
                finally:
                    env.pop('HAZEL_EDITOR_CAPTURE', None)
                    if saved is None: env.pop('HAZEL_TEST_METAL_EDITOR', None)
                    else: env['HAZEL_TEST_METAL_EDITOR'] = saved
                native = hz.ROOT/'build/testing/logs'/('macos-'+args.config+'-native-EditorSmoke.log')
                shutil.copy2(native, output/'workflow/native.log')
                text = native.read_text()
                report['gates']['workflow']['status'] = 'FAIL'
                if 'METAL_FINAL' not in text or 'M5_IMGUI_FINAL bytes=0 ' not in text or 'PASS: actual docked editor/panels' not in text:
                    raise RuntimeError('Metal editor workflow lacks native execution/retirement evidence')
                report['gates']['workflow'] = native_result(text)
            if args.only in (None, 'editor'):
                for name, project in [('authored-2d', 'Sandbox/Sandbox-Renderer2D.hproj'), ('authored-3d', 'RenderingReference/RenderingReference-Direct-PerspectiveUnlit.hproj'), ('authored-pbr', 'RenderingReference/RenderingReference-IBLQuality-Photo.hproj')]:
                    run([binaries/'Hazelnut/Hazelnut', '--smoke-test', '--metal', '--no-restore', '--project', hz.ROOT/'examples'/project], output/name, 'SMOKE PASS: application teardown completed', native=True)
    except Exception as error:
        report['error'] = str(error)
        raise
    finally:
        (output/'results.json').write_text(json.dumps(report, indent=2)+'\n')
    print('PASS focused M5 gates; full milestone remains INCOMPLETE pending remaining acceptance gates')


if __name__ == '__main__':
    main()
