#!/usr/bin/env python3
"""Serial native renderer verification tiers. Full is local acceptance, not platform CI."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import hazel as hz


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tier', choices=('fast', 'integration', 'full'), required=True)
    parser.add_argument('--config', choices=('Debug', 'Release'), action='append')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if hz.SYSTEM != 'macos':raise RuntimeError('Native renderer tiers require macOS; use hazel.py test for portable suites')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):raise RuntimeError('Use a fresh evidence directory')
    report = {'tier': args.tier, 'head': hz.output(['git', 'rev-parse', 'HEAD']),
              'worktree': hz.output(['git', 'status', '--short']), 'gates': {},
              'scope': 'Local native execution; clean acquisition, hosted platform CI and deployment certification remain separate'}
    started = time.monotonic()

    def run(name, command):
        command = list(map(str, command))
        begin = time.monotonic()
        gate = report['gates'][name] = {'command': command, 'status': 'FAIL'}
        try:
            with (output/(name+'.log')).open('w') as log:
                result = subprocess.run(command, cwd=hz.ROOT, stdout=log, stderr=subprocess.STDOUT)
            gate.update(seconds=time.monotonic()-begin, returncode=result.returncode)
            if result.returncode:raise RuntimeError('Gate failed: '+str(output/(name+'.log')))
            gate['status'] = 'PASS'
            print('PASS', name, f"{gate['seconds']:.2f}s", flush=True)
        finally:
            report['wallSeconds'] = time.monotonic()-started
            (output/'results.json').write_text(json.dumps(report, indent=2)+'\n')

    configs = args.config or (['Debug', 'Release'] if args.tier == 'full' else ['Release'])
    for config in dict.fromkeys(configs):
        # One build per configuration; downstream runners reuse its products and
        # checked offline artifacts. No runner is passed its own --build flag.
        targets = ['--target', 'MigrationSceneFoundationSmoke', '--target', 'Hazelnut', '--target', 'Nutella'] if args.tier == 'fast' else []
        run('build-'+config, [sys.executable, hz.ROOT/'scripts/hazel.py', 'build', '--tests', '--config', config, *targets])
        test = [sys.executable, hz.ROOT/'scripts/hazel.py', 'test', '--config', config]
        if args.tier == 'fast':
            run('policy-'+config, [*test, '--only', 'SceneFoundationSmoke'])
            continue
        run('opengl-'+config, test)
        resources = hz.binaries(config)/'Hazelnut/Resources'
        for script in ('metal_m3', 'metal_m4', 'metal_m2b1'):
            command = [sys.executable, hz.ROOT/f'scripts/macos/{script}.py', '--config', config,
                       '--output', output/(script+'-'+config)]
            if script != 'metal_m2b1':command += ['--resources', resources]
            run(script+'-'+config, command)
        run('metal_m5-'+config, [sys.executable, hz.ROOT/'scripts/macos/metal_m5.py', '--config', config,
                               '--output', output/('m5-'+config)])
        run('products-'+config, [sys.executable, hz.ROOT/'scripts/macos/metal_m6.py', '--config', config,
                                '--only', 'products', '--output', output/('products-'+config)])
        if args.tier == 'full':
            run('metal_m5-2x-'+config, [sys.executable, hz.ROOT/'scripts/macos/metal_m5.py', '--config', config,
                                      '--native-2x', '--output', output/('m5-2x-'+config)])
            for scale in ('1x', '2x'):
                run('lifecycle-'+config+'-'+scale, [sys.executable, hz.ROOT/'scripts/macos/metal_m1.py',
                    '--config', config, '--renderer3d', resources, '--output', output/('lifecycle-'+config+'-'+scale),
                    *(['--hidpi'] if scale == '2x' else [])])
    if args.tier == 'full':
        run('packages', [sys.executable, hz.ROOT/'scripts/macos/metal_m6.py', '--config', 'Release',
                        '--only', 'packages', '--output', output/'packages'])
    print('PASS selected local tier; other tiers, CI and unavailable hardware are not qualified')


if __name__ == '__main__':main()
