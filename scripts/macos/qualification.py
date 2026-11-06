"""Apple-driver regressions and safely isolated local package qualification."""
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
import time

import hazel as hz
from internal.testing.projects import snapshot


def native_profile(profile):
    if profile!='native':raise RuntimeError('macOS requires the real Apple OpenGL driver; Mesa profiles are unsupported')


def environment(working,resources,mono):
    from internal.package_tests import graphics_environment
    env=graphics_environment('native')
    for key in ('DYLD_LIBRARY_PATH','DYLD_FALLBACK_LIBRARY_PATH','HAZEL_FINAL_UI_PROJECT'):env.pop(key,None)
    env.update(HAZEL_RESOURCES=str(resources),HAZEL_MONO=str(mono),HAZEL_DATA=str(working/'profile'),
               HAZEL_SAVE_ROOT=str(working/'saves'),PATH='/usr/bin:/bin')
    return env


def logged(command,working,env,log,timeout=600):
    log.parent.mkdir(parents=True,exist_ok=True)
    print('TEST',log.stem,flush=True)
    with log.open('w',encoding='utf-8') as stream:
        result=subprocess.run(list(map(str,command)),cwd=working,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=timeout)
    evidence=log.read_text(errors='replace')
    if result.returncode or 'PASS:' not in evidence:
        raise RuntimeError('Qualification failed: '+str(log)+'\n'+evidence)
    return evidence


def games(configuration,profile,packages,output):
    native_profile(profile)
    if packages:return packaged_games(output)
    from internal.game_tests import model_tests
    model_tests(configuration)
    logs=hz.ROOT/'build/testing/games'/f'{configuration}-native'
    with tempfile.TemporaryDirectory(prefix='Hazel games é ') as folder:
        working=Path(folder)
        projects={name:snapshot(name,working/name) for name in ('MeadowRun','Skybound','LastLightkeeper','SceneTransitions','Sandbox')}
        for project in projects.values():hz.script_build(project,configuration)
        spec=importlib.util.spec_from_file_location('lightkeeper_authoring',hz.ROOT/'tests/examples/lightkeeper_authoring.py')
        audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)
        audit.check_project(projects['LastLightkeeper'])
        base=hz.binaries(configuration)
        env=environment(working,base/'Nutella/Resources',base/'Nutella/mono')
        env['DYLD_LIBRARY_PATH']=str(hz.mono_prefix()/'lib')
        evidence=logged([base/'MigrationExampleGamesSmoke/MigrationExampleGamesSmoke',projects['MeadowRun'],projects['Skybound'],
                        logs/'captures',projects['LastLightkeeper'],projects['SceneTransitions'],projects['Sandbox']],working,env,logs/'RuntimeSmoke.log')
        print('\n'.join(line for line in evidence.splitlines() if line.startswith('PASS:')),flush=True)
    print('PASS: native Apple example gameplay/render/UI/save/transition fixtures; product desktop interaction is separate acceptance',flush=True)


def packaged_games(output):
    from internal.package_tests import extract_verified
    from macos.packaging import relocate_executable,sign
    logs=hz.ROOT/'build/testing/games/Release-native'
    with tempfile.TemporaryDirectory(prefix='Hazel extracted games ü ') as folder:
        working=Path(folder);cwd=working/'unrelated cwd';cwd.mkdir()
        projects={};contents=None
        for name in ('MeadowRun','Skybound','LastLightkeeper','SceneTransitions','Sandbox'):
            root=extract_verified(output/f'{name}-macos-arm64-Release.zip',working)
            app=root/'Nutella.app';hz.run(['codesign','--verify','--deep','--strict',app])
            projects[name]=next((app/'Contents/Resources/Project').glob('*.hproj'))
            if contents is None:contents=app/'Contents'
        probe=contents/'MacOS/MigrationExampleGamesSmoke'
        hz.copy_changed(hz.binaries('Release')/'MigrationExampleGamesSmoke/MigrationExampleGamesSmoke',probe)
        relocate_executable(probe);sign(probe)
        env=environment(working,contents/'Resources',contents/'Resources/Mono');env['DYLD_PRINT_LIBRARIES']='1'
        text=logged(isolation_command([probe,projects['MeadowRun'],projects['Skybound'],working/'captures',
                    projects['LastLightkeeper'],projects['SceneTransitions'],projects['Sandbox']]),cwd,env,logs/'PackagedRuntimeSmoke.log')
        if any(str(hz.ROOT) in line or '/opt/homebrew/' in line for line in text.splitlines() if line.startswith(('dyld[','LOADED_IMAGE '))):
            raise RuntimeError('Packaged example fixture loaded producer native libraries')
        print('\n'.join(line for line in text.splitlines() if line.startswith('PASS:')),flush=True)
        hz.copy_tree(working/'captures',logs/'packaged captures')
    print('PASS: all five extracted game projects, packaged runtime/resources, real Apple rendering and native gameplay/save/scene-transition checks with source/SDK denied',flush=True)


def isolation_command(command,source_sdk=None):
    # A per-process read restriction, never renaming/hiding real SDKs or changing
    # global security settings. Children inherit it, including Mono's dlopens.
    import json
    roots=[hz.ROOT.resolve(),Path('/opt/homebrew'),Path('/usr/local/Cellar'),
           Path('/Library/Frameworks/Mono.framework'),Path('/usr/local/lib/mono')]
    if source_sdk is not None:roots.append(source_sdk.resolve())
    profile='(version 1)(allow default)'+''.join('(deny file-read* (subpath '+json.dumps(str(p),ensure_ascii=False)+'))' for p in roots)
    return ['/usr/bin/sandbox-exec','-p',profile,*map(str,command)]


def product_acceptance(name,bundle,working,logs,source_sdk,interactive=False):
    from internal.package_tests import graphics_environment
    env=graphics_environment('native')
    for key in ('DYLD_LIBRARY_PATH','DYLD_FALLBACK_LIBRARY_PATH','HAZEL_FINAL_UI_PROJECT'):env.pop(key,None)
    env.update(HAZEL_DATA=str(working/'profile'),HAZEL_SAVE_ROOT=str(working/'saves'),PATH='/usr/bin:/bin')
    # No resource/Mono overrides: exercise the real product's installation layout.
    mode='Desktop' if interactive else 'Smoke'
    log=logs/(name+'-'+mode+'.log')
    if interactive:
        print('INTERACTIVE:',name,'— source/SDK-isolated desktop launch. Exercise prepared content, then close normally (10 minute limit). Private profile:',env['HAZEL_DATA'],flush=True)
    else:print('AUTOMATED PRODUCT:',name,'— source/SDK denied; real frames and guarded normal close',flush=True)
    if not interactive:
        # Reject unattended launches before application construction if private
        # storage was omitted. Never fall back to the owner's normal profile.
        unsafe=env.copy();unsafe.pop('HAZEL_DATA')
        result=subprocess.run(isolation_command([bundle/'Contents/MacOS'/name,'--smoke-test'],source_sdk),
                              cwd=working,env=unsafe,capture_output=True,text=True,timeout=20)
        rejection=result.stdout+result.stderr
        (logs/(name+'-SmokeGuard.log')).write_text(rejection,encoding='utf-8')
        if result.returncode!=1 or '--smoke-test requires absolute HAZEL_DATA and HAZEL_SAVE_ROOT' not in rejection or 'Creating window' in rejection:
            raise RuntimeError('Product did not reject missing private smoke-test storage before window creation')
    with log.open('w',encoding='utf-8') as stream:
        command=[bundle/'Contents/MacOS'/name,*([] if interactive else ['--smoke-test'])]
        process=subprocess.Popen(isolation_command(command,source_sdk),cwd=working,env=env,stdout=stream,stderr=subprocess.STDOUT)
        print('OWNED PRODUCT PID:',process.pid,'LOG:',log,flush=True)
        images=set();deadline=time.monotonic()+(600 if interactive else 120)
        try:
            # Read the owned process's mapped native files without debugger
            # entitlements or dyld environment flags. Shared-cache OS images
            # need not have individual vnode mappings; packaged dylibs do.
            while process.poll() is None:
                if time.monotonic()>=deadline:raise RuntimeError(mode+' product acceptance timed out; no normal-close pass was recorded')
                mapped=subprocess.run(['/usr/sbin/lsof','-a','-p',str(process.pid),'-d','txt','-Fn'],capture_output=True,text=True,timeout=5)
                if mapped.returncode and process.poll() is None:raise RuntimeError('Cannot inspect owned product native mappings: '+mapped.stderr)
                for line in mapped.stdout.splitlines():
                    if line.startswith('n') and (line.endswith('.dylib') or '/Contents/MacOS/' in line):images.add(line[1:])
                time.sleep(.1)
            status=process.returncode
        finally:
            if process.poll() is None:process.kill();process.wait()
            # Retain useful diagnostics even on timeout; this is evidence of
            # an attempted launch, never a normal-shutdown acceptance marker.
            (logs/(name+'-'+mode+'-NativeMappings.log')).write_text('\n'.join(sorted(images))+'\n')
    text=log.read_text(errors='replace')
    if status or name+' ready:' not in text or 'Terminating GLFW' not in text:
        raise RuntimeError(mode+' product did not start/close normally: '+str(log)+'\n'+text)
    if not interactive and ('SMOKE: 60 rendered frames' not in text or 'SMOKE PASS: application teardown completed' not in text):
        raise RuntimeError('Product did not prove rendered frames and completed teardown: '+str(log))
    shipped=[bundle/'Contents/MacOS'/name,*((bundle/'Contents/Frameworks').glob('*.dylib'))]
    identities={(p.stat().st_dev,p.stat().st_ino) for p in shipped}
    invalid=[]
    for path in images:
        if path.startswith(('/System/Library/','/usr/lib/','/Library/Apple/System/',
                            '/System/Volumes/Preboot/Cryptexes/OS/System/Library/',
                            '/System/Volumes/Preboot/Cryptexes/OS/usr/lib/')):continue
        info=Path(path).stat()
        if (info.st_dev,info.st_ino) not in identities:invalid.append(path)
    if not any('libmonosgen' in line for line in images) or invalid:
        raise RuntimeError(mode+' product native closure failed: '+str(log)+'; unexpected images: '+str(invalid))
    hz.run(['codesign','--verify','--deep','--strict',bundle])
    print('PASS:',name,mode,'actual relocated product startup/normal shutdown, runtime-loaded closure and unchanged signature; human interaction details remain separately recorded',flush=True)


def packages(output,profile,interactive=False,interactive_app=None):
    native_profile(profile)
    hz.macos_toolchain()
    sdk=hz.mono_prefix()
    from internal.package_tests import extract_verified
    from macos.packaging import audit_binary,relocate_executable,sign
    logs=hz.ROOT/'build/testing/packages/macos';logs.mkdir(parents=True,exist_ok=True)
    # Preserve each desktop attempt, including failures, without overwriting a
    # previous app's diagnostics when qualifying the products separately.
    desktop_logs=Path(tempfile.mkdtemp(prefix='desktop-',dir=logs)) if interactive else None
    if interactive_app:
        other='Nutella' if interactive_app=='Hazelnut' else 'Hazelnut'
        print('NOT EXECUTED:',other,'desktop acceptance; only',interactive_app,'was selected',flush=True)
    with tempfile.TemporaryDirectory(prefix='Hazel relocated ü space ') as folder:
        working=Path(folder)
        # Prove the restriction before accepting any package evidence.
        denied=subprocess.run(isolation_command(['/bin/cat',hz.ROOT/'premake5.lua'],sdk),capture_output=True)
        if denied.returncode==0:raise RuntimeError('Package test isolation did not deny source SDK access')
        for name in ('Hazelnut','Nutella'):
            cwd=working/name/'unrelated cwd';cwd.mkdir(parents=True)
            root=extract_verified(output/f'{name}-macos-arm64-Release.zip',working)
            bundle=root/(name+'.app');contents=bundle/'Contents';native=contents/'Frameworks';mono=contents/'Resources/Mono'
            hz.run(['codesign','--verify','--deep','--strict',bundle])
            for binary in [contents/'MacOS'/name,*native.glob('*.dylib')]:audit_binary(binary,native)
            product_acceptance(name,bundle,cwd,logs,sdk)
            if interactive and (interactive_app is None or interactive_app==name):
                product_acceptance(name,bundle,cwd,desktop_logs,sdk,interactive=True)
            env=environment(working/name,contents/'Resources',mono)
            if name=='Hazelnut':
                from internal.package_tests import native_import_helper
                source=(hz.ROOT/'examples/RenderingReference/Assets/Reference/Import_v1/Sources/SlotsAndInstances.glb').read_bytes()
                native_import_helper(contents/'MacOS/HazelProject',contents/'Resources',cwd,env,source,
                                     isolation_command([],sdk))
            env['DYLD_PRINT_LIBRARIES']='1'
            hz.copy_tree(contents/'Resources',cwd/'assets')
            hz.copy_tree(hz.ROOT/'tests/fixtures/typography',cwd/'typography')
            if name=='Nutella':
                for args,needle in [(['--help'],'StartScene'),(['--list-projects'],'.hproj')]:
                    result=subprocess.run(isolation_command([contents/'MacOS'/name,*args],sdk),cwd=cwd,env=env,capture_output=True,text=True,timeout=20)
                    if result.returncode or needle not in result.stdout:raise RuntimeError('Relocated runtime CLI failed: '+result.stderr)
            managed=contents/'Resources/Scripts/MonoEmbeddingFixture.dll'
            hz.run([*hz.mono_command(sdk),'-target:library','-out:'+str(managed),hz.ROOT/'tests/macos/feasibility/mono/ManagedFixture.cs'])
            embedding=contents/'MacOS/MonoEmbeddingProbe'
            hz.run(['xcrun','clang++','-std=c++17','-arch','arm64','-mmacosx-version-min=27.0',
                    '-I'+str(sdk/'include/mono-2.0'),hz.ROOT/'tests/macos/feasibility/mono/host.cpp',
                    '-L'+str(native),'-lmonosgen-2.0','-Wl,-rpath,@executable_path/../Frameworks','-o',embedding])
            sign(embedding)
            helpers=logged(isolation_command([embedding,mono,managed,contents/'Resources/Scripts/Hazel-ScriptCore.dll'],sdk),cwd,env,logs/(name+'-MonoHelpers.log'))
            if not all(value in helpers for value in ('libmonosgen','libMonoPosixHelper','libmono-native')):
                raise RuntimeError('Package dynamic Mono helper closure was not exercised')
            # Copies of maintained native regression executables consume exactly
            # the extracted runtime/resources. Original bundle signatures above
            # are checked before adding test-only probes to this disposable copy.
            for test in ('MigrationRenderer2DSmoke','MigrationRuntimeSessionSmoke'):
                probe=contents/'MacOS'/test;hz.copy_changed(hz.binaries('Release')/test/test,probe)
                relocate_executable(probe);sign(probe)
                args=[probe]
                if test=='MigrationRuntimeSessionSmoke':
                    args += [contents/'Resources/Scripts/Hazel-ScriptCore.dll',hz.binaries('Release')/'MigrationManagedFixture/MigrationManagedFixture.dll']
                    fixture=contents/'Resources/Scripts/Regression.dll';hz.copy_changed(args[-1],fixture);args[-1]=fixture
                text=logged(isolation_command(args,sdk),cwd,env,logs/(name+'-'+test+'.log'))
                loads=[line for line in text.splitlines() if line.startswith(('dyld[','LOADED_IMAGE '))]
                forbidden=[line for line in loads if str(hz.ROOT) in line or '/opt/homebrew/' in line or '/Cellar/' in line]
                if forbidden or not any('libmonosgen' in line for line in loads):raise RuntimeError('Relocated dynamic closure failed: '+str(forbidden))
            print('PASS:',name,'extracted signature/checksums, arm64/static closure, denied-source Apple rendering/picking and managed RuntimeSession lifecycle',flush=True)
    if not interactive:print('MANUAL ACCEPTANCE: --interactive adds physical dialogs/input/window checks; automated product frames do not establish those interactions',flush=True)
