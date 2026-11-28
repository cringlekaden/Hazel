#!/usr/bin/env python3
"""Bake Pavilion probes and run the native deterministic camera study on macOS."""
import argparse
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT.parents[1]
sys.path.insert(0, str(ROOT/'scripts'))
import hazel


def run(command, output, env):
    print(' '.join(map(str, command)), flush=True)
    with output.open('w') as log:
        subprocess.run(command, cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT,
                       check=True, timeout=1200)


def native(backend, config, descriptor, output, values):
    resources = hazel.binaries(config)/'Nutella/Resources'
    env = {k:v for k,v in os.environ.items() if not k.startswith(('HAZEL_', 'DYLD_'))}
    env.update(HAZEL_RESOURCES=str(resources), HAZEL_DATA=str(output/'profile'),
               DYLD_LIBRARY_PATH=str(hazel.mono_prefix()/'lib'),
               HAZEL_RENDERING_REFERENCE_PROJECT=str(descriptor), HAZEL_REFLECTION_DIAGNOSTIC='1',
               **values)
    target = 'MetalRenderer2DQuadSmoke' if backend == 'metal' else 'MigrationRenderer2DSmoke'
    options = [resources, output, '--reflection-probes'] if backend == 'metal' else ['--reflection-probes', output]
    output.mkdir(parents=True, exist_ok=True)
    run([hazel.binaries(config)/target/target,*options],output/'run.log',env)
    return env


def bake(descriptor, config, output):
    # The native capture records six views and checks view-effect invariance.
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='Pavilion probe authoring ') as temp:
        staged = Path(temp)/'Project'
        shutil.copytree(descriptor.parent, staged, ignore=shutil.ignore_patterns('*.prepare.lock'))
        candidate = staged/descriptor.name
        start = re.search(r'^  StartScene: (.+)$', candidate.read_text(), re.M).group(1)
        scene = staged/'Assets'/start
        disable_probes = r'(  ReflectionProbeComponent:\n    Environment: [^\n]+\n    Enabled: )true'
        scene.write_text(re.sub(disable_probes, r'\g<1>false', scene.read_text()))
        env = native('metal',config,candidate,output,dict(HAZEL_PAVILION_STUDY='bake'))
    folder = descriptor.parent/'Assets/Pavilion/Reflections'
    folder.mkdir(parents=True,exist_ok=True)
    for name in ('Hall','Archive'):
        raw = folder/f'{name}.hlight'
        shutil.copyfile(output/f'{name}.hlight', raw)
        ref = f'Pavilion/Reflections/{name}.henv'
        (folder/f'{name}.henv').write_text(f"""Environment:
  Version: 1
  Name: Pavilion {name} / baked
  Notice: See NOTICE-Pavilion.md for the source asset licenses.
  Source:
    Kind: SceneCapture
    Image: Pavilion/Reflections/{name}.hlight
    Interpretation: LinearRec709D65Cube
  Preparation:
    Recipe: Hazel.IBL_v1.GGX-correlated-single.r045.E-pi.AB-NoV-r.cube-top
    Background: 16
    Specular: 256
    Irradiance: 32
    Samples: 1024
""")
        run([hazel.binaries(config)/'HazelProject/HazelProject','prepare-environment',descriptor,ref],
            output/f'prepare-{name}.log',env)
        accepted=(folder/f'{name}.henv.accepted').read_text()
        spec=re.search(r'^\s*Path: (.*?/Specular\.hlight)$',accepted,re.M).group(1)
        mip_zero=raw.read_bytes()[72:]
        if (descriptor.parent/'Assets'/spec).read_bytes()[72:72+len(mip_zero)] != mip_zero:
            raise RuntimeError(f'{name}: cooked mip zero differs from the native capture')


def stage(descriptor, output):
    start = re.search(r'^  StartScene: (Scenes/Pavilion(?:-High|-Diagnostic)?\.hazel)$',
                      descriptor.read_text(), re.M)
    if not start:
        raise ValueError('Stage expects one of the three Pavilion descriptors.')
    output.mkdir(parents=True, exist_ok=True)
    for name in (descriptor.name, 'NOTICE-Pavilion.md'):
        shutil.copy2(descriptor.parent/name, output/name)
    for subtree in ('Pavilion', 'Prepared', 'Materials/wood_table_001', 'Native/Direct_v1'):
        shutil.copytree(descriptor.parent/'Assets'/subtree, output/'Assets'/subtree,
                        ignore=shutil.ignore_patterns('*.glb', '*.hdr', '*.prepare.lock',
                                                      'Hall.hlight', 'Archive.hlight'))
    scene = output/'Assets'/start.group(1)
    scene.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(descriptor.parent/'Assets'/start.group(1), scene)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode',choices=('bake','stage','stills','pairs','tour','loop','loop-check','loop-pairs','shimmer','benchmark','present','editor-resize',*(f'{i:02}' for i in range(1,10))))
    parser.add_argument('--backend',choices=('metal','opengl'),default='metal')
    parser.add_argument('--config',choices=('Debug','Release'),default='Release')
    parser.add_argument('--project', type=Path, default=PROJECT/'RenderingShowcase.hproj')
    parser.add_argument('--width',type=int,choices=(640,960,1280,1920,2560,3840),default=960,
                        help='Offscreen physical width. With present, 2560 adds a fullscreen 1440p stage.')
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    output=args.output.resolve()
    if output.exists() and any(output.iterdir()):
        parser.error('Use an empty evidence directory.')
    descriptor=args.project.resolve()
    if output.is_relative_to(descriptor.parent/'Assets'):
        parser.error('Write study output outside the project Assets directory.')
    if args.mode=='bake':
        bake(descriptor,args.config,output)
    elif args.mode=='stage':
        stage(descriptor,output)
    elif args.mode=='present':
        from macos.local_lighting_presented import presented_timings
        from macos.qualification import native_console
        if args.backend != 'metal':
            parser.error('Positive presentation timestamps require Metal 4.')
        if args.width not in (960, 2560):
            parser.error('Present measures 720p and 1080p by default. Add --width 2560 for 1440p.')
        output.mkdir(parents=True, exist_ok=True)
        native_console(output/'console.json')
        env = {k:v for k,v in os.environ.items() if not k.startswith(('HAZEL_', 'DYLD_', 'MTL_'))}
        env.update(HAZEL_DATA=str(output/'profile'), HAZEL_SAVE_ROOT=str(output/'saves'),
                   DYLD_LIBRARY_PATH=str(hazel.mono_prefix()/'lib'), HAZEL_METAL_TRACE='1',
                   HAZEL_METAL_MEMORY_TRACE='1', HAZEL_LOCAL_PRESENT_BENCHMARK='1')
        extents = ('1280x720','1920x1080')
        if args.width == 2560:
            env['HAZEL_PRESENT_1440'] = '1'
            extents += ('2560x1440',)
        resources = hazel.binaries(args.config)/'Nutella/Resources'
        binary = hazel.binaries(args.config)/'MetalM1Smoke/MetalM1Smoke'
        log = output/'present.log'
        run([binary,output,'--benchmark-metal',resources,descriptor,'-1'], log, env)
        result = presented_timings(log, descriptor.name, extents)
        (output/'presentation.json').write_text(json.dumps(result,indent=2)+'\n')
    elif args.mode=='editor-resize':
        output.mkdir(parents=True, exist_ok=True)
        env = os.environ.copy()
        env.update(HAZEL_DATA=str(output/'profile'), HAZEL_SAVE_ROOT=str(output/'saves'),
                   DYLD_LIBRARY_PATH=str(hazel.mono_prefix()/'lib'), HAZEL_METAL_TRACE='1')
        run([hazel.binaries(args.config)/'MigrationEditorSmoke/MigrationEditorSmoke',
             '--pavilion-resize',descriptor,hazel.binaries(args.config)/'Hazelnut/Resources',
             args.backend,output/'editor.ppm'], output/'run.log',env)
    else:
        native(args.backend,args.config,descriptor,output,dict(
            HAZEL_PAVILION_STUDY=args.mode, HAZEL_PAVILION_WIDTH=str(args.width),
            **({'HAZEL_METAL_TRACE':'1','HAZEL_METAL_MEMORY_TRACE':'1'} if args.mode=='benchmark' else {})))
    (output/'invocation.json').write_text(json.dumps(dict(mode=args.mode, backend=args.backend,
        configuration=args.config, width=args.width, project=str(descriptor)),indent=2)+'\n')


if __name__=='__main__':
    main()
