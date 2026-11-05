"""Maintained native CLI fixture checks inside the existing sequential runner."""
import hashlib
import json
from pathlib import Path
import subprocess

import hazel as hz


def reference_imports(configuration, working, env):
    source=hz.ROOT/'examples/RenderingReference/Assets/Reference/Import_v1'
    metadata=json.loads((source/'sources.json').read_text())
    frozen=json.loads((source/'outputs.json').read_text())
    for name,digest in frozen['files'].items():
        if hashlib.sha256((source/name).read_bytes()).hexdigest()!=digest:raise RuntimeError('Frozen native import artifact changed: '+name)
    helper=hz.binaries(configuration)/'HazelProject'/('HazelProject'+hz.EXECUTABLE_SUFFIX)
    project=working/'Native Import é';project.parent.mkdir(parents=True,exist_ok=True)
    def run(arguments):
        return subprocess.run([str(helper),*map(str,arguments)],env=env,cwd=working,text=True,encoding='utf-8',capture_output=True,timeout=60)
    result=run(['create','--name','Native Import Qualification','--identifier','ImportQualification','--destination',project,'--templates',hz.ROOT/'Hazel/Resources/Templates'])
    if result.returncode:raise RuntimeError(result.stdout+result.stderr)
    descriptor=project/'ImportQualification.hproj'
    for name,case in metadata['cases'].items():
        file=source/'Sources'/name
        if hashlib.sha256(file.read_bytes()).hexdigest()!=case['sha256']:raise RuntimeError('Frozen import source changed: '+name)
        destination=Path('Imported')/file.stem
        result=run(['import-glb',descriptor,file,destination])
        if (result.returncode==0)!=case['accepted']:raise RuntimeError(name+': '+result.stdout+result.stderr)
        if not case['accepted']:
            if case['diagnostic'].lower() not in (result.stdout+result.stderr).lower():raise RuntimeError('Missing actionable diagnostic for '+name+': '+result.stderr)
            if (project/'Assets'/destination).exists():raise RuntimeError('Rejected import published partial content: '+name)
        print('PASS: native import fixture',name,'accepted' if case['accepted'] else 'rejected with diagnostic',flush=True)
    assets=project/'Assets'
    inventory=working/'native-import-inventory.txt'
    files=sorted(p.relative_to(assets).as_posix() for p in assets.rglob('*') if p.is_file() and 'Intermediates' not in p.relative_to(assets).parts)
    inventory.write_text('\n'.join(files)+'\n',encoding='utf-8')
    auditor=hz.binaries(configuration)/'SpriteAssetAudit'/('SpriteAssetAudit'+hz.EXECUTABLE_SUFFIX)
    result=subprocess.run([str(auditor),str(assets),str(inventory),'--project',str(descriptor)],env=env,cwd=working,text=True,encoding='utf-8',capture_output=True,timeout=60)
    if result.returncode:raise RuntimeError('Native import closure failed: '+result.stdout+result.stderr)
    for file in assets.rglob('*.glb'):file.unlink()
    result=subprocess.run([str(auditor),str(assets),str(inventory),'--project',str(descriptor)],env=env,cwd=working,text=True,encoding='utf-8',capture_output=True,timeout=60)
    if result.returncode:raise RuntimeError('Cooked assets required original source/parser: '+result.stdout+result.stderr)
    print('PASS: generated native material/mesh/prefab/artifact closure, including source-free native audit; no script compilation',flush=True)
