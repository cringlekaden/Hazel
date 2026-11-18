#!/usr/bin/env python3
"""M6 actual product and package matrix, reusing the maintained bundle isolation gates.

Build Debug/Release with --tests first. This matrix is only one part of M6;
CI, numerical/lifetime regressions and matched timings remain separate gates.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import time
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import hazel as hz
from internal.testing.projects import snapshot
from macos.metal_m5 import native_result
from macos.qualification import native_console

SCENES = {
    '2d': ('Sandbox', 'Sandbox-Renderer2D.hproj'),
    '3d': ('RenderingReference', 'RenderingReference-Direct-PerspectiveUnlit.hproj'),
    'pbr': ('RenderingReference', 'RenderingReference-IBLQuality-Photo.hproj'),
}


def products(configuration, output):
    from macos.metal_shaders import produce
    shaders = output/'shaders'
    produce(configuration, shaders)
    for app in ('Hazelnut', 'Nutella'):
        hz.copy_tree(shaders, hz.binaries(configuration)/app/'Resources/shaders/metal')
    results = {}
    for scene, (example, descriptor) in SCENES.items():
        project = snapshot(example, output/'projects'/scene, descriptor)
        for app in ('Hazelnut', 'Nutella'):
            for backend in ('opengl', 'metal'):
                key = f'{app}-{scene}-{backend}'
                folder = output/key;folder.mkdir()
                native_console(folder/'console-before.json')
                env = os.environ.copy()
                for name in list(env):
                    if name.startswith(('HAZEL_', 'DYLD_', 'MTL_')):env.pop(name, None)
                env.update(HAZEL_DATA=str(folder/'profile'), HAZEL_SAVE_ROOT=str(folder/'saves'),
                           HAZEL_METAL_TRACE='1', MTL_DEBUG_LAYER='1', MTL_SHADER_VALIDATION='1')
                command = [hz.binaries(configuration)/app/app, '--smoke-test', '--project', project]
                if app == 'Hazelnut':command += ['--no-restore']
                if backend == 'metal':command += ['--metal']
                (folder/'command.json').write_text(json.dumps(list(map(str, command)), indent=2))
                with (folder/'run.log').open('w') as log:
                    start = time.monotonic()
                    process = subprocess.Popen(list(map(str, command)), cwd=folder, env=env, stdout=log, stderr=subprocess.STDOUT)
                    ready = None
                    try:
                        while process.poll() is None:
                            if ready is None and app+' ready:' in (folder/'run.log').read_text(errors='replace'):
                                ready = time.monotonic()-start
                            if time.monotonic()-start > 120:raise RuntimeError('Product timeout: '+key)
                            time.sleep(.02)
                    finally:
                        if process.poll() is None:process.kill();process.wait()
                text = (folder/'run.log').read_text(errors='replace')
                if process.returncode or 'SMOKE PASS: application teardown completed' not in text:
                    raise RuntimeError('Product failed: '+str(folder/'run.log'))
                result = native_result(text, imgui=app=='Hazelnut') if backend=='metal' else {'status': 'PASS'}
                result.update(wallSeconds=time.monotonic()-start, readyObservedSeconds=ready,
                              startupBoundary='Process spawn to ready log observed at 20ms polling; not first presentation')
                results[key] = result
                (output/'results.json').write_text(json.dumps(results, indent=2)+'\n')
                print('PASS', key, flush=True)


def packages(output):
    from macos.packaging import package
    from internal.packaging import load_project, project_files, validate_sprites
    from macos.qualification import packages as qualify
    results = {}
    for scene, (example, descriptor) in SCENES.items():
        project = snapshot(example, output/'projects'/scene, descriptor)
        _, _, assets = load_project(project)
        if validate_sprites(assets, list(project_files(assets)), project):hz.script_build(project, 'Release')
        folder = output/scene
        package('all', project, folder)
        # Qualify the unchanged, signed actual products before disposable probes.
        qualify(folder, 'native', logs=folder/'qualification')
        results[scene] = {'status': 'PASS', 'backends': ['opengl', 'metal'],
                          'products': ['Hazelnut', 'Nutella']}
        (output/'results.json').write_text(json.dumps(results, indent=2)+'\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', choices=('Debug', 'Release'), default='Release')
    parser.add_argument('--only', choices=('products', 'packages'), required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):raise RuntimeError('Use a fresh evidence directory')
    if args.only == 'packages' and args.config != 'Release':raise RuntimeError('Deployment bundles are Release only')
    hz.yaml_tools()
    if args.only == 'products':products(args.config, output)
    else:packages(output)
    print('PASS M6 '+args.only+' subset; other M6 gates remain separate')


if __name__ == '__main__':main()
