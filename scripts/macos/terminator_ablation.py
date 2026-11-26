#!/usr/bin/env python3
"""Localize the smooth-horizon IBL regression without modifying production resources."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import subprocess
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import hazel as hz
from internal.testing.projects import snapshot

BASELINE = 'b49256001bd6d13422fa4c88d0a52a96ac3b5c42'


def analytic():
    def dot(a, b):
        return sum(x*y for x, y in zip(a, b))
    def unit(a):
        return tuple(x/math.sqrt(dot(a, a)) for x in a)
    def reflection(n, v):
        return tuple(2*dot(n, v)*x-y for x, y in zip(n, v))
    def resolve(n, g, v):
        c = dot(n, g)
        if dot(g, v) <= 0 or c > 0 and dot(reflection(n, v), g) >= 0:
            return n
        t = unit(tuple(x-c*y for x, y in zip(n, g)))
        lo, hi = 0, min(math.acos(max(-1, min(1, c))), math.pi/2)
        for _ in range(64):
            mid = (lo+hi)/2
            q = tuple(x*math.cos(mid)+y*math.sin(mid) for x, y in zip(g, t))
            if dot(reflection(q, v), g) >= 0:
                lo = mid
            else:
                hi = mid
        return unit(tuple(x*math.cos(lo)+y*math.sin(lo) for x, y in zip(g, t)))
    n, g, v = unit((-.1, 0, 1)), (0, 0, 1), unit((1, 0, .02))
    result = {}
    for name, horizon in [('baseline', g), ('resolve-only', n), ('separated', g)]:
        q = resolve(n, horizon, v)
        result[name] = dict(normal=q, nv=dot(q, v), reflectionGeometryCosine=dot(reflection(q, v), g))
    assert result['resolve-only']['reflectionGeometryCosine'] < -.17
    assert result['separated']['nv'] > 0
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', choices=('Debug', 'Release'), default='Debug')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    if any(out.iterdir()):
        raise RuntimeError('Use a fresh evidence directory')
    source = hz.ROOT/'Hazel/Resources'
    baseline = subprocess.check_output(['git', 'show', BASELINE+':Hazel/Resources/shaders/ForwardPBR.glsl']).decode()
    original = 'N=resolveShadingNormal(N,geometricNormal,V);'
    assert baseline.count(original) == 1
    # Change ONLY the indirect normal-repair input. Keep every direct-light
    # gate, BRDF, energy equation, texture and reference expectation fixed.
    ablation = baseline.replace(original, 'N=resolveShadingNormal(N,baseNormal,V);')
    reference = snapshot('RenderingReference', out/'reference', 'RenderingReference.hproj')
    showcase = snapshot('RenderingShowcase', out/'showcase', 'RenderingShowcase.hproj')
    report = dict(baseline=BASELINE, analytic=analytic(), configuration=args.config, gates={})
    try:
        for name, shader, expected in [('baseline', baseline, 0), ('resolve-only', ablation, 1),
                                      ('separated', (source/'shaders/ForwardPBR.glsl').read_text(), 0)]:
            folder = out/name
            shutil.copytree(source, folder/'Resources')
            (folder/'Resources/shaders/ForwardPBR.glsl').write_text(shader)
            env = os.environ.copy()
            env.update(HAZEL_RESOURCES=str(folder/'Resources'), HAZEL_DATA=str(folder/'profile'),
                       HAZEL_RENDERING_REFERENCE_PROJECT=str(reference), HAZEL_RENDERING_SHOWCASE_PROJECT=str(showcase),
                       DYLD_LIBRARY_PATH=str(hz.mono_prefix()/'lib'))
            command = [hz.binaries(args.config)/'MigrationRenderer2DSmoke/MigrationRenderer2DSmoke', '--ibl-horizon']
            with (folder/'run.log').open('w') as log:
                result = subprocess.run(list(map(str, command)), cwd=hz.ROOT, env=env,
                                        stdout=log, stderr=subprocess.STDOUT, timeout=240)
            log = (folder/'run.log').read_text()
            gate = report['gates'][name] = dict(returncode=result.returncode, expected=expected,
                sourceSHA256=hashlib.sha256(shader.encode()).hexdigest(), command=list(map(str, command)))
            if result.returncode != expected or (expected and 'Shading normal differs from independent geometric-horizon constraint' not in log):
                raise RuntimeError('Unexpected ablation outcome: '+name)
            gate['status'] = 'PASS expected failure reproduced' if expected else 'PASS'
            print(name, gate['status'], flush=True)
        report['status'] = 'PASS localized to indirect resolve input; historical IBL checks unchanged'
    except Exception as error:
        report.update(status='INCOMPLETE', error=str(error))
        raise
    finally:
        (out/'results.json').write_text(json.dumps(report, indent=2)+'\n')


if __name__ == '__main__':
    main()
