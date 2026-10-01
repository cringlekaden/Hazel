#!/usr/bin/env python3
"""Run the migration's fixed Linux desktop checks sequentially, preserving logs."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', choices=('Debug', 'Release'), required=True)
    parser.add_argument('--stage', choices=('stage3', 'stage4a', 'stage4b', 'stage4c', 'stage4d', 'stage4e', 'stage4f', 'stage5', 'stage6', 'stage7', 'stage8', 'final'), default='stage3')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    binaries = root / 'bin' / f'{args.config}-linux-x86_64'
    evidence = root / 'build/migration/evidence'
    evidence.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='hazel-desktop-checks-') as directory:
        working = Path(directory)
        shutil.copytree(root / 'Sandbox/assets', working / 'assets')
        settings = root / 'Sandbox/imgui.ini'
        if settings.exists():
            shutil.copyfile(settings, working / 'imgui.ini')
        profiles = (
            ('native', {}),
            ('gl41', {'MESA_GL_VERSION_OVERRIDE': '4.1', 'MESA_GLSL_VERSION_OVERRIDE': '410'}),
            ('software', {'LIBGL_ALWAYS_SOFTWARE': '1', 'LP_NUM_THREADS': '2'}),
        )
        if args.stage in ('stage4a', 'stage4d', 'stage4e'):
            profiles = profiles[:1]
        elif args.stage == 'stage4b':
            profiles = profiles[:2]
        for profile, overrides in profiles:
            if args.stage == 'final':
                checks = ['RendererFeaturesSmoke', 'Renderer2DSmoke', 'FontSmoke', 'SceneGPUSmoke', 'EditorSmoke', 'CoreSmoke', 'Sandbox', 'Hazelnut']
                if profile == 'native':
                    checks += ['RendererSmoke', 'ShaderToolsSmoke', 'SceneFoundationSmoke', 'ProjectPhysicsSmoke', 'MonoSmoke', 'SceneSmoke']
            elif args.stage == 'stage8':
                checks = ['EditorSmoke', 'SceneGPUSmoke', 'RendererFeaturesSmoke']
                if profile == 'native': checks += ['SceneSmoke', 'CoreSmoke', 'Sandbox']
            elif args.stage == 'stage6':
                checks = ['SceneGPUSmoke', 'RendererFeaturesSmoke']
                if profile == 'native':
                    checks += ['SceneSmoke', 'ProjectPhysicsSmoke', 'CoreSmoke', 'Sandbox']
            elif args.stage in ('stage4f', 'stage7'):
                checks = ['SceneGPUSmoke', 'Renderer2DSmoke', 'RendererFeaturesSmoke']
                if profile == 'native':
                    checks += ['SceneSmoke', 'FontSmoke', 'SceneFoundationSmoke', 'CoreSmoke', 'MonoSmoke', 'Sandbox']
                elif profile == 'gl41':
                    checks += ['Sandbox']
            elif args.stage == 'stage4a':
                checks = ['SceneFoundationSmoke', 'CoreSmoke', 'Sandbox']
            elif args.stage == 'stage4e':
                checks = ['MonoSmoke', 'Sandbox']
            elif args.stage == 'stage4d':
                checks = ['ProjectPhysicsSmoke', 'Sandbox']
            elif args.stage == 'stage4b':
                checks = ['FontSmoke']
                if profile == 'native':
                    checks += ['SceneFoundationSmoke', 'CoreSmoke', 'Sandbox']
            elif args.stage in ('stage4c', 'stage5'):
                checks = ['Renderer2DSmoke', 'RendererFeaturesSmoke']
                if profile == 'native':
                    checks += ['FontSmoke', 'SceneFoundationSmoke', 'CoreSmoke', 'Sandbox', 'RendererSmoke', 'ShaderToolsSmoke']
                elif profile == 'gl41':
                    checks += ['Sandbox']
            else:
                checks = ['RendererFeaturesSmoke']
                if profile != 'software':
                    checks += ['CoreSmoke', 'Sandbox']
                if profile == 'native':
                    checks += ['RendererSmoke', 'ShaderToolsSmoke']
            for name in checks:
                target = name if name in ('Sandbox', 'Hazelnut') else 'Migration' + name
                executable = binaries / target / target
                command = [str(executable)]
                if name in ('MonoSmoke', 'SceneSmoke', 'SceneGPUSmoke'):
                    command += [str(binaries / 'Hazel-ScriptCore/Hazel-ScriptCore.dll'),
                                str(binaries / 'MigrationManagedFixture/MigrationManagedFixture.dll')]
                if name == 'EditorSmoke':
                    command += [str(binaries / 'Hazel-ScriptCore/Hazel-ScriptCore.dll'),
                                str(binaries / 'SandboxScripts/Sandbox.dll'), str(root / 'Hazelnut')]
                if name in ('Sandbox', 'Hazelnut'):
                    command = [sys.executable, str(root / 'scripts/migration/sandbox-smoke.py'),
                               str(executable), '--assets', str(root / name), '--require-order']
                    if name == 'Hazelnut': command += ['--editor']
                log = evidence / f'{args.stage}-{args.config.lower()}-{profile}-{name}.log'
                environment = os.environ.copy()
                # A native profile must not inherit another test's overrides.
                for variable in ('MESA_GL_VERSION_OVERRIDE', 'MESA_GLSL_VERSION_OVERRIDE',
                                 'LIBGL_ALWAYS_SOFTWARE', 'LP_NUM_THREADS'):
                    environment.pop(variable, None)
                environment.update(overrides)
                print(f'RUN {args.config} {profile} {name}: {log.relative_to(root)}', flush=True)
                with log.open('w') as output:
                    result = subprocess.run(command, cwd=working, env=environment,
                                            stdout=output, stderr=subprocess.STDOUT, timeout=120)
                print(f'EXIT {result.returncode}', flush=True)
                if result.returncode:
                    print(log.read_text(errors='replace'), flush=True)
                    return result.returncode
    return 0


if __name__ == '__main__':
    sys.exit(main())
