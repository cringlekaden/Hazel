"""Sequential regression runner, with isolated optional graphics test drivers."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import urllib.request
import hazel as hz


def software_driver():
    pin = json.loads((hz.ROOT/'scripts/internal/testing/windows-mesa.json').read_text())
    archive = hz.ROOT/'build/testing/windows-mesa.7z'; archive.parent.mkdir(parents=True,exist_ok=True)
    if not archive.exists(): urllib.request.urlretrieve(pin['url'],archive)
    if hashlib.sha256(archive.read_bytes()).hexdigest()!=pin['sha256']: raise RuntimeError('Windows test driver checksum mismatch')
    extracted=hz.ROOT/'build/testing/windows-mesa'
    if not (extracted/'x64/opengl32.dll').is_file():
        seven=shutil.which('7z') or str(Path(os.environ['ProgramFiles'])/'7-Zip/7z.exe')
        hz.run([seven,'x','-y',archive,'-o'+str(extracted)])
    return extracted/'x64'


def test(configuration, profile='native'):
    base=hz.binaries(configuration)
    logs=hz.ROOT/'build/testing/logs';logs.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='hazel tests space-é-') as temporary:
        working=Path(temporary)
        hz.copy_tree(hz.ROOT/'Hazel/Resources',working/'assets')
        hz.copy_tree(hz.ROOT/'Hazelnut/Resources/Icons',working/'assets/Icons')
        hz.copy_changed(hz.ROOT/'Hazelnut/Resources/imgui.ini',working/'assets/imgui.ini')
        hz.copy_tree(hz.ROOT/'tests/fixtures/assets',working/'assets')
        hz.copy_tree(hz.ROOT/'tests/fixtures/AuthoringProject',working/'AuthoringProject')
        hz.copy_changed(base/'MigrationManagedFixture/MigrationManagedFixture.dll',working/'AuthoringProject/Assets/Scripts/Binaries/Regression.dll')
        env=os.environ.copy()
        for key in ('MESA_GL_VERSION_OVERRIDE','MESA_GLSL_VERSION_OVERRIDE','LIBGL_ALWAYS_SOFTWARE','GALLIUM_DRIVER','LP_NUM_THREADS'):
            env.pop(key,None)
        if profile=='gl41': env.update(MESA_GL_VERSION_OVERRIDE='4.1',MESA_GLSL_VERSION_OVERRIDE='410')
        if profile=='software':env.update(LIBGL_ALWAYS_SOFTWARE='1',GALLIUM_DRIVER='llvmpipe',LP_NUM_THREADS='2')
        env['HAZEL_RESOURCES']=str(working/'assets')
        env['HAZEL_MONO']=str(base/'Nutella/mono')
        if hz.SYSTEM=='linux':env['LD_LIBRARY_PATH']=str(hz.mono_prefix()/'lib')
        private=working/'executables';private.mkdir()
        if hz.SYSTEM=='windows' and profile in ('software','gl41'):
            for dll in software_driver().glob('*.dll'): hz.copy_changed(dll,private/dll.name)
            env.update(GALLIUM_DRIVER='llvmpipe',LP_NUM_THREADS='2',MESA_SHADER_CACHE_DIR=str(working/'mesa-cache'))
        names=('ShaderToolsSmoke','SceneFoundationSmoke','ProjectPhysicsSmoke','MonoSmoke','SceneSmoke','CoreSmoke','RendererSmoke','RendererFeaturesSmoke','Renderer2DSmoke','FontSmoke','SceneGPUSmoke','RuntimeSessionSmoke','EditorSmoke')
        for name in names:
            target='Migration'+name;suffix='.exe' if hz.SYSTEM=='windows' else ''
            executable=base/target/(target+suffix)
            if not executable.is_file():raise RuntimeError('Missing '+target+'; run build --tests first')
            if hz.SYSTEM=='windows': hz.copy_changed(executable,private/executable.name);executable=private/executable.name
            command=[executable]
            if name in ('MonoSmoke','SceneSmoke','SceneGPUSmoke','RuntimeSessionSmoke'):command += [base/'Hazel-ScriptCore/Hazel-ScriptCore.dll',base/'MigrationManagedFixture/MigrationManagedFixture.dll']
            if name=='EditorSmoke':command += [base/'Hazel-ScriptCore/Hazel-ScriptCore.dll',base/'MigrationManagedFixture/MigrationManagedFixture.dll',working]
            env['HAZEL_DATA']=str(working/'data'/name)
            log=logs/f'{hz.SYSTEM}-{configuration}-{profile}-{name}.log'
            print('TEST',name,flush=True)
            with log.open('w',encoding='utf-8') as out:
                result=subprocess.run(list(map(str,command)),cwd=working,env=env,stdout=out,stderr=subprocess.STDOUT,timeout=240)
            if result.returncode:
                print(log.read_text(errors='replace'));raise RuntimeError(f'{name} failed ({result.returncode}); log: {log}')
            if 'PASS:' not in log.read_text(errors='replace'):raise RuntimeError('Missing acceptance evidence: '+str(log))
        print(f'PASS: {configuration} {profile}, {len(names)} sequential regression executables')
