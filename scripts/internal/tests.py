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


def launcher_checks():
    """Normal F5 permits session restore; intentional runtime arguments stay explicit."""
    for platform in ('linux', 'windows'):
        data = json.loads((hz.ROOT/'scripts/internal/vscode'/platform/'launch.json').read_text(encoding='utf-8'))
        configs = data['configurations']
        editor = next(c for c in configs if c['name'].startswith('Hazelnut'))
        runtime = next(c for c in configs if c['name'].startswith('Nutella'))
        if editor['args']:
            raise RuntimeError(platform + ' default Hazelnut F5 overrides the remembered project')
        if runtime['args'] != ['--project', '${workspaceFolder}/examples/SceneTransitions/SceneTransitions.hproj']:
            raise RuntimeError(platform + ' Nutella development example launch changed')
    print('PASS: Linux/Windows F5 leaves editor project selection to session restore; runtime example remains explicit', flush=True)


def typography_inputs(working):
    """Pinned CPU-only fixtures never become application/game resources."""
    from internal.packaging import licenses
    expected={'DejaVuSans.ttf':'7da195a74c55bef988d0d48f9508bd5d849425c1770dba5d7bfc6ce9ed848954',
              'LICENSE-DejaVu.txt':'7a083b136e64d064794c3419751e5c7dd10d2f64c108fe5ba161eae5e5958a93'}
    for file, digest in expected.items():
        source=hz.ROOT/'tests/fixtures/typography'/file
        if hashlib.sha256(source.read_bytes()).hexdigest()!=digest:
            raise RuntimeError('Typography reference bytes changed: '+file)
    hz.copy_tree(hz.ROOT/'tests/fixtures/typography',working/'typography')
    for source in ('Hazel/vendor/libunibreak/src/GraphemeBreakTest.txt',
                   'Hazel/vendor/libunibreak/src/LineBreakTest.txt',
                   'Hazel/vendor/SheenBidi/Tools/Unicode/BidiCharacterTest.txt'):
        file=hz.ROOT/source
        if '17.0.0' not in file.read_text(encoding='utf-8').splitlines()[0]:
            raise RuntimeError('Unexpected Unicode conformance version: '+source)
        hz.copy_changed(file,working/'typography'/file.name)
    package=working/'notice-closure';licenses(package)
    for name,file in (('harfbuzz','COPYING'),('libunibreak','LICENCE'),('SheenBidi','LICENSE')):
        if (package/'licenses/vendor'/name/file).read_bytes()!=(hz.ROOT/'Hazel/vendor'/name/file).read_bytes():
            raise RuntimeError('Typography notice closure mismatch: '+name)
    unicode='scripts/dependencies/licenses/typography/LICENSE-Unicode.txt'
    if (package/'licenses/typography/LICENSE-Unicode.txt').read_bytes()!=(hz.ROOT/unicode).read_bytes():
        raise RuntimeError('Unicode notice closure mismatch')
    if any(file.name=='DejaVuSans.ttf' for file in package.rglob('*')):
        raise RuntimeError('Test-only face leaked into application closure')
    print('PASS: pinned test face/Unicode versions and linked typography notice closure; no fixture font shipped',flush=True)


def sdk_readiness_checks(working):
    """Production preflight with controlled SDK paths, without changing host SDKs."""
    from unittest.mock import patch
    from internal.authoring import preflight
    if hz.SYSTEM == 'linux':
        installed=hz.mono_prefix()
        selected=working/'Mono SDK space é'/'usr';selected.mkdir(parents=True)
        for name in ('bin','lib','include'):
            (selected/name).symlink_to(installed/name, target_is_directory=True)
        config=installed.parent/'etc' if installed!=Path('/usr') else Path('/etc')
        (selected.parent/'etc').symlink_to(config, target_is_directory=True)
        defaults={base/name for base in (hz.ROOT/'build/dependencies/mono/linux/usr',Path('/usr'))
                  for name in ('lib/mono/4.5/mcs.exe','lib/mono/4.7.2-api/mscorlib.dll')}
        original=Path.is_file;consulted=[];unavailable=set()
        def controlled(file):
            if file in defaults:
                consulted.append(file);return False
            return file not in unavailable and original(file)
        resolver=hz.mono_prefix
        with patch.dict(os.environ,{'HAZEL_MONO_SDK':str(selected)}), patch.object(Path,'is_file',controlled):
            with patch.object(hz,'mono_prefix',wraps=resolver) as resolve:
                preflight('scripts')
                assert resolve.call_count==1, 'Readiness resolved a second SDK'
            assert resolver()==selected and not consulted, 'Custom SDK ignored or default SDK consulted'
            from internal.authoring import create_project
            descriptor=create_project('Custom SDK é','CustomSDKProbe',working/'custom compiler project é')
            hz.script_build(descriptor,'Debug')
            assert (descriptor.parent/'Assets/Scripts/Binaries/CustomSDKProbe.dll').is_file()
            for name in ('lib/mono/4.5/mcs.exe','lib/mono/4.7.2-api/mscorlib.dll'):
                unavailable.add(selected/name)
                try:preflight('scripts')
                except RuntimeError as error:
                    assert str(selected) in str(error) and name in str(error), str(error)
                else:raise RuntimeError('Incomplete explicitly selected SDK was accepted')
                runtime=hz.mono_command(selected)[:-1]
                assert subprocess.run(list(map(str,[*runtime,'--version'])),capture_output=True).returncode==0
                unavailable.clear()
            assert not consulted, 'Incomplete override silently fell back to a default SDK'
        print('PASS: real custom Mono SDK with spaces/Unicode, default candidates unavailable, one readiness resolution, compiled project scripts, missing compiler/profile rejection and independent runtime',flush=True)
    else:
        # Windows compilation uses MSBuild/reference assemblies, not Linux mcs.
        from internal import authoring
        pack=working/'Targeting SDK space é'
        reference=pack/'Reference Assemblies/Microsoft/Framework/.NETFramework/v4.7.2/mscorlib.dll'
        reference.parent.mkdir(parents=True);reference.write_bytes(b'controlled targeting availability')
        with patch.dict(os.environ,{'ProgramFiles(x86)':str(pack)}), \
             patch.object(authoring,'native_contract',return_value=hz.mono_prefix()), \
             patch.object(hz,'vs_toolchain') as compiler, patch.object(hz,'diagnose') as diagnose:
            preflight('scripts');compiler.assert_called_with(False)
            preflight('export');compiler.assert_called_with(True);diagnose.assert_called_once()
            reference.unlink()
            try:preflight('scripts')
            except RuntimeError as error:assert '.NET Framework 4.7.2' in str(error)
            else:raise RuntimeError('Missing Windows targeting pack accepted')
        print('PASS: Windows script/export compiler selection and missing managed targeting-pack diagnostics',flush=True)


def authoring_checks(working):
    """Exercise the same transactional project service invoked by editor jobs."""
    import yaml
    from internal.authoring import create_project
    from internal.child_tools import prepare
    destination=working/'New project space é'
    saved_path=os.environ.get('PATH','')
    try:
        os.environ['PATH']=''
        prepare(['git','cmd'] if hz.SYSTEM=='windows' else ['git','make'])
        create_project('A portable garden é','GardenProbe',destination)
        assert not (destination/'Assets/Scripts/Binaries/GardenProbe.dll').exists()
        hz.script_build(destination/'GardenProbe.hproj','Debug')
    finally:os.environ['PATH']=saved_path
    descriptor=destination/'GardenProbe.hproj'
    config=yaml.safe_load(descriptor.read_text(encoding='utf-8'))['Project']
    scene=yaml.safe_load((destination/'Assets/Scenes/Start.hazel').read_text(encoding='utf-8'))
    assert config['StartScene']=='Scenes/Start.hazel' and config['ScriptModulePath']=='Scripts/Binaries/GardenProbe.dll'
    assert (destination/'Assets'/config['ScriptModulePath']).is_file()
    assert any(e.get('CameraComponent',{}).get('Primary') for e in scene['Entities'])
    original=descriptor.read_bytes()
    try:create_project('Overwrite','GardenProbe',destination)
    except RuntimeError:pass
    else:raise RuntimeError('New Project overwrote an existing destination')
    assert descriptor.read_bytes()==original
    failed=working/'Failed project'
    compiler=hz.script_build
    def fail(*args):raise RuntimeError('Controlled initial assembly failure')
    try:
        hz.script_build=fail
        try:create_project('Compiler failure','FailureProbe',failed,build=True)
        except RuntimeError:pass
        else:raise RuntimeError('Controlled script build failure was not reported')
    finally:hz.script_build=compiler
    assert (failed/'FailureProbe.hproj').is_file() and not (failed/'Assets/Scripts/Binaries/FailureProbe.dll').exists()
    print('PASS: canonical native creation without PATH, uncompiled editing contract, later build, existing destination and failed-build content retention',flush=True)


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
    launcher_checks()
    base=hz.binaries(configuration)
    logs=hz.ROOT/'build/testing/logs';logs.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='hazel tests space-é-') as temporary:
        working=Path(temporary)
        hz.yaml_tools();sdk_readiness_checks(working);authoring_checks(working)
        hz.copy_tree(hz.ROOT/'Hazel/Resources',working/'assets')
        hz.copy_tree(hz.ROOT/'Hazelnut/Resources/Icons',working/'assets/Icons')
        hz.copy_changed(hz.ROOT/'Hazelnut/Resources/imgui.ini',working/'assets/imgui.ini')
        hz.copy_tree(hz.ROOT/'tests/fixtures/assets',working/'assets')
        typography_inputs(working)
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
        names=('TypographySmoke','GameUIModelSmoke','GameUIIntegrationSmoke','ShaderToolsSmoke','SpriteSmoke','SceneFoundationSmoke','ProjectPhysicsSmoke','MonoSmoke','SceneSmoke','CoreSmoke','RendererSmoke','RendererFeaturesSmoke','Renderer2DSmoke','FontSmoke','SceneGPUSmoke','RuntimeSessionSmoke','EditorSmoke')
        for name in names:
            target='Migration'+name;suffix='.exe' if hz.SYSTEM=='windows' else ''
            executable=base/target/(target+suffix)
            if not executable.is_file():raise RuntimeError('Missing '+target+'; run build --tests first')
            if hz.SYSTEM=='windows': hz.copy_changed(executable,private/executable.name);executable=private/executable.name
            command=[executable]
            if name in ('SpriteSmoke','MonoSmoke','SceneSmoke','SceneGPUSmoke','RuntimeSessionSmoke'):command += [base/'Hazel-ScriptCore/Hazel-ScriptCore.dll',base/'MigrationManagedFixture/MigrationManagedFixture.dll']
            if name=='EditorSmoke':command += [base/'Hazel-ScriptCore/Hazel-ScriptCore.dll',base/'MigrationManagedFixture/MigrationManagedFixture.dll',working]
            env['HAZEL_DATA']=str(working/'data'/name)
            log=logs/f'{hz.SYSTEM}-{configuration}-{profile}-{name}.log'
            print('TEST',name,flush=True)
            with log.open('w',encoding='utf-8') as out:
                # Full native structural/presentation authoring is bounded work;
                # its unoptimized Debug run on Grandpa exceeds the old 240s cap.
                limit=600 if name=='EditorSmoke' and configuration=='Debug' else 240
                result=subprocess.run(list(map(str,command)),cwd=working,env=env,stdout=out,stderr=subprocess.STDOUT,timeout=limit)
            if result.returncode:
                print(log.read_text(errors='replace'));raise RuntimeError(f'{name} failed ({result.returncode}); log: {log}')
            evidence=log.read_text(errors='replace')
            if 'PASS:' not in evidence:raise RuntimeError('Missing acceptance evidence: '+str(log))
            if name=='EditorSmoke' and env.get('HAZEL_FINAL_UI_PROJECT'):
                tutorial='PASS: native tutorial Inspector, composed creation/duplicate/delete/property undo, prefab instantiate, save/reopen and repeated Play/Stop in a private project'
                if tutorial not in evidence:raise RuntimeError('Selected tutorial authoring did not execute: '+str(log))
                print(tutorial,flush=True)
        print(f'PASS: {configuration} {profile}, {len(names)} sequential regression executables')
