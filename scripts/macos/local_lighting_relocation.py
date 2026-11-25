#!/usr/bin/env python3
"""Check packaged local-light compute inventory and source/SDK-isolated execution."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import hazel as hz
from internal.package_tests import extract_verified
from macos.qualification import isolation_command,native_console
from macos.packaging import relocate_executable,sign

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--archives',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    args=p.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    if any(out.iterdir()):raise RuntimeError('Use a fresh evidence directory')
    native_console(out/'console.json')
    working=Path(tempfile.mkdtemp(prefix='Hazel local lights relocated ü '));report=dict(working=str(working),gates={})
    try:
        bundles={}
        for name in ('Nutella','Hazelnut'):
            root=extract_verified(args.archives.resolve()/(name+'-macos-arm64-Release.zip'),working/name)
            bundle=root/(name+'.app');bundles[name]=bundle;resources=bundle/'Contents/Resources'
            metadata=json.loads((resources/'build.json').read_text())
            for artifact,digest in metadata['metal_shaders'].items():
                if hashlib.sha256((resources/'shaders/metal'/artifact).read_bytes()).hexdigest()!=digest:raise RuntimeError('Exported shader hash differs: '+artifact)
            if 'LocalLightCull.metallib' not in metadata['metal_shaders'] or list(resources.rglob('*.metal')):raise RuntimeError('Local-light artifact/source inventory differs')
            subprocess.run(['codesign','--verify','--deep','--strict',str(bundle)],check=True,capture_output=True)
            report['gates'][name+'-inventory-signature']='PASS'
        helper=working/'probe/MacOS/LocalLightProbe';helper.parent.mkdir(parents=True)
        shutil.copy2(hz.binaries('Release')/'MetalRenderer2DQuadSmoke/MetalRenderer2DQuadSmoke',helper)
        shutil.copytree(bundles['Nutella']/'Contents/Frameworks',working/'probe/Frameworks');relocate_executable(helper);sign(helper)
        project=working/'examples/RenderingReference';shutil.copytree(bundles['Nutella']/'Contents/Resources/Project',project)
        descriptor=next(project.glob('*.hproj'))
        # Mesh retirement's established independent fixture opens this descriptor.
        shutil.copy2(hz.ROOT/'examples/RenderingReference/RenderingReference-Direct-PerspectivePBR.hproj',project/'RenderingReference-Direct-PerspectivePBR.hproj')
        resources=bundles['Nutella']/'Contents/Resources';sdk=hz.mono_prefix()
        env={k:v for k,v in os.environ.items() if not k.startswith(('HAZEL_','DYLD_','MTL_'))}
        env.update(PATH='/nonexistent',DEVELOPER_DIR='/nonexistent',MTL_DEBUG_LAYER='1',MTL_SHADER_VALIDATION='1',
            HAZEL_RENDERING_REFERENCE_PROJECT=str(descriptor),HAZEL_DATA=str(working/'profile'),HAZEL_SAVE_ROOT=str(working/'saves'))
        cmd=isolation_command([helper,resources,working/'probe-results','--local-lights'],sdk,metal=True)
        with (out/'probe.log').open('w') as log:r=subprocess.run(cmd,cwd=working,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
        if r.returncode:raise RuntimeError('Isolated local-light probe failed')
        report['gates']['source-SDK-isolated-numerical-culling-retirement']='PASS'
        library=resources/'shaders/metal/LocalLightCull.metallib';saved=library.with_suffix('.absent');library.rename(saved)
        try:
            cmd=isolation_command([helper,resources,working/'missing-results','--local-lights'],sdk,metal=True)
            r=subprocess.run(cmd,cwd=working,env=env,capture_output=True,text=True,timeout=90)
            (out/'missing-library.log').write_text(r.stdout+r.stderr)
            if r.returncode!=1 or 'Compute metallib is missing or exceeds 16 MiB' not in r.stdout+r.stderr:raise RuntimeError('Missing culling library did not reject explicitly')
            report['gates']['missing-culling-library-rejection']='PASS'
        finally:saved.rename(library)
        for name,bundle in bundles.items():
            for backend in ('metal','opengl'):
                localEnv=env.copy();localEnv.pop('HAZEL_RENDERING_REFERENCE_PROJECT',None)
                localEnv.update(HAZEL_DATA=str(working/(name+'-profile')),HAZEL_SAVE_ROOT=str(working/(name+'-saves')))
                selected=next((bundle/'Contents/Resources/Project').glob('*.hproj'))
                cmd=[bundle/'Contents/MacOS'/name,'--project',selected,'--'+backend,'--smoke-test']
                if name=='Hazelnut':cmd.append('--no-restore')
                cmd=isolation_command(cmd,sdk,metal=True)
                with (out/(name+'-'+backend+'.log')).open('w') as log:r=subprocess.run(list(map(str,cmd)),cwd=working,env=localEnv,stdout=log,stderr=subprocess.STDOUT,timeout=90)
                if r.returncode:raise RuntimeError('Isolated packaged product failed: '+name+' '+backend)
                report['gates'][name+'-source-free-'+backend+'-presentation']='PASS'
        report['status']='PASS native exported inventory, isolated light probe and products; owner visual gate separate'
    except Exception as error:report['status']='INCOMPLETE';report['error']=str(error);raise
    finally:(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    print(report['status'])
if __name__=='__main__':main()
