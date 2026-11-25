#!/usr/bin/env python3
"""Validate exported XeGTAO inventory and a source/SDK-isolated native AO probe.

Real exported product presentation remains a separate unlocked-desktop gate.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import hazel as hz
from internal.package_tests import extract_verified
from macos.qualification import isolation_command
from macos.packaging import relocate_executable, sign


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--taa',action='store_true',help='Exercise exported TAA motion/history instead of the AO-only probe')
    parser.add_argument('--archives', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve(); output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        raise RuntimeError('Use a fresh evidence directory')
    working = Path(tempfile.mkdtemp(prefix='Hazel XeGTAO relocated ü '))
    report = dict(working=str(working), gates={}, productPresentation='NOT RUN: separate unlocked native product gate')
    try:
        bundles = {}
        for name in ('Nutella','Hazelnut'):
            archive = args.archives.resolve() / (name + '-macos-arm64-Release.zip')
            root = extract_verified(archive, working / name)
            bundle = root / (name + '.app'); bundles[name] = bundle
            resources = bundle / 'Contents/Resources'
            metadata = json.loads((resources / 'build.json').read_text())
            for artifact, digest in metadata['metal_shaders'].items():
                if hashlib.sha256((resources / 'shaders/metal' / artifact).read_bytes()).hexdigest() != digest:
                    raise RuntimeError('Exported shader hash differs: ' + artifact)
            if not (resources / 'licenses/XeGTAO/LICENSE').is_file() or list(resources.rglob('*.metal')):
                raise RuntimeError('Exported XeGTAO license/authoring-source inventory differs')
            brdf=resources/'BRDF/GGX-MS_v2.hlight'
            canonical=hz.ROOT/'Hazel/Resources/BRDF/GGX-MS_v2.hlight'
            if not brdf.is_file() or brdf.read_bytes()!=canonical.read_bytes():
                raise RuntimeError('Exported canonical direct-only BRDF table differs')
            signed = subprocess.run(['codesign','--verify','--deep','--strict',str(bundle)], capture_output=True, text=True)
            if signed.returncode:
                raise RuntimeError(signed.stderr)
            report['gates'][name + '-inventory-signature'] = dict(status='PASS', artifacts=metadata['metal_shaders'], sourceDirty=metadata['source_dirty'])
        # The helper consumes the exported resources and project; canonical source,
        # SDK, Homebrew, developer compilers and MSL authoring sources are denied.
        source = hz.binaries('Release') / 'MetalRenderer2DQuadSmoke/MetalRenderer2DQuadSmoke'
        helper = working / 'probe/MacOS/XeGTAOProbe'; helper.parent.mkdir(parents=True)
        shutil.copy2(source, helper)
        shutil.copytree(bundles['Nutella'] / 'Contents/Frameworks', working / 'probe/Frameworks')
        relocate_executable(helper);sign(helper)
        project = working / 'examples/RenderingReference'
        shutil.copytree(bundles['Nutella'] / 'Contents/Resources/Project', project)
        # Focused independent geometry also opens this existing descriptor.
        for descriptor in ('RenderingReference-Direct-PerspectivePBR.hproj','RenderingReference-XeGTAO.hproj'):
            shutil.copy2(hz.ROOT / 'examples/RenderingReference' / descriptor, project / descriptor)
        resources = bundles['Nutella'] / 'Contents/Resources'
        env = {k:v for k,v in os.environ.items() if not k.startswith(('HAZEL_','DYLD_','MTL_'))}
        env.update(PATH='/nonexistent', DEVELOPER_DIR='/nonexistent', MTL_DEBUG_LAYER='1', MTL_SHADER_VALIDATION='1')
        sdk = hz.mono_prefix()
        command = isolation_command([helper, resources, working / 'probe-results', '--taa' if args.taa else '--xegtao'], sdk, metal=True)
        with (output / 'probe.log').open('w') as log:
            result = subprocess.run(command, cwd=working, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=120)
        if result.returncode:
            raise RuntimeError('Isolated exported AO probe failed; inspect probe.log')
        report['gates']['isolated-exported-temporal-probe' if args.taa else 'isolated-exported-AO-probe'] = dict(status='PASS', command=list(map(str,command)), helperSHA256=hashlib.sha256(source.read_bytes()).hexdigest(), scope='Offscreen production AO/lighting/UI/ID/depth/lifetime checks, not a pristine product presentation')
        libraryName='TemporalAA' if args.taa else 'XeGTAO'
        library = resources / ('shaders/metal/'+libraryName+'.metallib')
        saved = library.with_suffix('.absent')
        library.rename(saved)
        try:
            command = isolation_command([helper, resources, working / 'missing-results', '--taa' if args.taa else '--xegtao'], sdk, metal=True)
            result = subprocess.run(command, cwd=working, env=env, capture_output=True, text=True, timeout=60)
            text = result.stdout + result.stderr
            (output / 'missing-library.log').write_text(text)
            if result.returncode != 1 or ('Missing/oversized '+libraryName+' metallib') not in text:
                raise RuntimeError('Missing XeGTAO library did not reject explicitly')
            report['gates']['missing-library-rejection'] = dict(status='PASS', scope='Process-level rejection; no same-device recovery claim')
        finally:
            saved.rename(library)
        brdf=resources/'BRDF/GGX-MS_v2.hlight'
        absent=brdf.with_suffix('.absent');brdf.rename(absent)
        try:
            command=isolation_command([helper,resources,working/'missing-brdf','--taa' if args.taa else '--xegtao'],sdk,metal=True)
            result=subprocess.run(command,cwd=working,env=env,capture_output=True,text=True,timeout=60)
            text=result.stdout+result.stderr;(output/'missing-brdf.log').write_text(text)
            if result.returncode!=1 or 'GGX-MS_v2.hlight' not in text:
                raise RuntimeError('Missing canonical BRDF table did not reject explicitly')
            report['gates']['missing-canonical-BRDF-rejection']=dict(status='PASS',scope='No zero-LUT or alternate-BRDF fallback')
        finally:
            absent.rename(brdf)
        result = subprocess.run(['codesign','--verify','--deep','--strict',str(bundles['Nutella'])], capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError('Restored exported signature differs: ' + result.stderr)
        report['status'] = 'PASS focused exported inventory/relocation probe; actual product acceptance remains separate'
    except Exception as error:
        report['status'] = 'INCOMPLETE'; report['error'] = str(error)
        raise
    finally:
        (output / 'results.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
