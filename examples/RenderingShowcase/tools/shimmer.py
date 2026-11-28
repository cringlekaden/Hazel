#!/usr/bin/env python3
"""Measure display luma variation in a matched near-subpixel floor sweep."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

import numpy as np
from PIL import Image
import yaml

import study


def metrics(folder):
    frames = sorted(folder.glob('sample-*.ppm'))
    if len(frames) != 32:
        raise RuntimeError(f'Expected 32 recorded frames in {folder}')
    images = np.stack([np.asarray(Image.open(frame), dtype=np.float32) for frame in frames])
    h, w = images.shape[1:3]
    luma = images[:, int(h*.65):int(h*.94), int(w*.16):int(w*.87)] @ [.2126,.7152,.0722]
    change = np.abs(np.diff(luma, axis=0))
    return {'rmsFrameDelta': float(np.sqrt(np.mean(change**2))),
            'p99FrameDelta': float(np.quantile(change,.99)),
            'pixelsAboveFourCodes': float(np.mean(change >= 4))}


def control(project, variant):
    scene = project.parent/'Assets/Scenes/Pavilion.hazel'
    data = yaml.safe_load(scene.read_text())
    p = data['Presentation']
    if variant in ('no-taa', 'no-ao-no-taa'):
        p['TemporalAA'] = False
    if variant in ('no-ao', 'no-ao-no-taa'):
        p['AmbientOcclusion']['Enabled'] = False
    elif variant == 'ao-quality-2':
        p['AmbientOcclusion']['Quality'] = 2
    elif variant == 'no-ssr':
        p['ScreenReflections']['Enabled'] = False
    elif variant == 'dense-ssr':
        p['ScreenReflections']['Steps'] = 80
    elif variant == 'no-global':
        p['Environment']['Lighting'] = False
    elif variant == 'no-probes':
        for e in data['Entities']:
            if 'ReflectionProbeComponent' in e:
                e['ReflectionProbeComponent']['Enabled'] = False
    elif variant == 'no-direct':
        for e in data['Entities']:
            for light in ('PointLightComponent','SpotLightComponent','DirectionalLightComponent'):
                if light in e:
                    e[light]['Enabled'] = False
    scene.write_text(yaml.safe_dump(data,sort_keys=False),encoding='utf-8')
    if variant in ('no-normals', 'roughness-point-mip'):
        for path in (project.parent/'Assets/Pavilion').rglob('*.hmat'):
            data = yaml.safe_load(path.read_text())
            material = data['Material']
            if variant == 'no-normals':
                material['NormalScale'] = 0
            elif material['Textures']['MetallicRoughness']['Image']:
                material['Textures']['MetallicRoughness']['Min'] = 'LinearMipmapNearest'
            path.write_text(yaml.safe_dump(data,sort_keys=False),encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--project',type=Path,default=study.PROJECT/'RenderingShowcase.hproj')
    parser.add_argument('--backend', choices=('metal','opengl'),default='metal')
    parser.add_argument('--variants',nargs='+',default=['authored','no-taa','no-ssr','no-probes',
                        'no-global','no-direct','no-normals','roughness-point-mip','dense-ssr',
                        'no-ao','no-ao-no-taa','ao-quality-2'])
    args=parser.parse_args()
    out=args.output.resolve()
    out.mkdir(parents=True,exist_ok=True)
    report={}
    for variant in args.variants:
        with tempfile.TemporaryDirectory(prefix='Pavilion shimmer ') as folder:
            root=Path(folder)/'project'
            study.stage(args.project.resolve(),root)
            project=root/args.project.name
            if variant != 'authored': control(project,variant)
            run=out/variant
            subprocess.run([sys.executable,Path(study.__file__),'shimmer','--width','960',
                            '--project',project,'--backend',args.backend,'--output',run],check=True)
            report[variant]=metrics(run)
            match=re.search(r'PAVILION_SHIMMER temporalPhase=(\d+)',(run/'run.log').read_text())
            report[variant]['temporalPhase']=int(match.group(1))
            (out/'metrics.json').write_text(json.dumps(report,indent=2)+'\n')


if __name__ == '__main__':
    main()
