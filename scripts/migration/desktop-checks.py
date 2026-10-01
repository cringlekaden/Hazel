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
    parser.add_argument('--stage', choices=('stage3', 'stage4a'), default='stage3')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    binaries = root / 'bin' / f'{args.config}-linux-x86_64'
    evidence = root / 'docs/migration/evidence'
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
        if args.stage == 'stage4a':
            profiles = profiles[:1]
        for profile, overrides in profiles:
            if args.stage == 'stage4a':
                checks = ['SceneFoundationSmoke', 'CoreSmoke', 'Sandbox']
            else:
                checks = ['RendererFeaturesSmoke']
                if profile != 'software':
                    checks += ['CoreSmoke', 'Sandbox']
                if profile == 'native':
                    checks += ['RendererSmoke', 'ShaderToolsSmoke']
            for name in checks:
                target = name if name == 'Sandbox' else 'Migration' + name
                executable = binaries / target / target
                command = [str(executable)]
                if name == 'Sandbox':
                    command = [sys.executable, str(root / 'scripts/migration/sandbox-smoke.py'),
                               str(executable), '--assets', str(root / 'Sandbox'), '--require-order']
                log = evidence / f'{args.stage}-{args.config.lower()}-{profile}-{name}.log'
                environment = os.environ.copy()
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
