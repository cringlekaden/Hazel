"""Logical/native gameplay contracts plus bounded application/package smoke."""
import contextlib
import hashlib
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile
import zlib
import hazel as hz
from internal.package_tests import wait_for,unavailable_sources,extract_verified,graphics_environment,cli
from internal.testing.desktop import Desktop
from internal.tests import software_driver

GAMES=('MeadowRun','Skybound','LastLightkeeper')
NATIVE_GAMES=GAMES[:2] # Retain the existing two-game strict native regression contract.

def model_tests(configuration):
    directory=hz.ROOT/'tests/examples';destination=hz.ROOT/'build/testing/managed'/f'{configuration}-{hz.SYSTEM}'
    env=os.environ.copy();env['HAZEL_GAME_TEST_OUTPUT']=destination.as_posix()
    hz.run([hz.premake(),'vs2022' if hz.SYSTEM=='windows' else 'gmake'],cwd=directory,env=env)
    projects=destination/'Projects'
    if hz.SYSTEM=='windows':command=[hz.vs_toolchain()[1],projects/'ExampleGameTests.sln','/m:2',f'/p:Configuration={configuration}','/p:Platform=x64']
    else:command=['make',f'config={configuration.lower()}','-j2','CSC='+shlex.join(map(str,hz.mono_command(hz.mono_prefix())))]
    hz.run(command,cwd=projects,env=env)
    command=[destination/'ExampleGameTests.exe']
    if hz.SYSTEM=='linux':command=hz.mono_command(hz.mono_prefix())[:-1]+command
    hz.run(command)


def ppm_to_png(source):
    header,size,maximum,pixels=source.read_bytes().split(b'\n',3)
    width,height=map(int,size.split())
    if header!=b'P6' or maximum!=b'255' or len(pixels)!=width*height*3:raise RuntimeError('Invalid framebuffer capture')
    def chunk(kind,body):return struct.pack('>I',len(body))+kind+body+struct.pack('>I',zlib.crc32(kind+body))
    rows=b''.join(b'\0'+pixels[y*width*3:(y+1)*width*3] for y in range(height))
    source.with_suffix('.png').write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(rows))+chunk(b'IEND',b''))


def exercise(desktop,executable,project,app,game,working,env,logs,shots,packaged=False):
    """Short owned-window lifecycle smoke; gameplay lives in logical/native tests."""
    log=logs/f'{game}-{app}.log'
    files=[project]+list(project.parent.glob('Assets/Scenes/*.hazel'))
    before={p:hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
    command=[str(executable)]
    if not packaged:command+=(['--project'] if app=='Nutella' else [])+[str(project)]
    window=None
    with log.open('w',encoding='utf-8') as stream:
        process=subprocess.Popen(command,cwd=working,env=env,stdout=stream,stderr=subprocess.STDOUT)
        def content():return log.read_text(errors='replace')
        try:
            window=wait_for(process,lambda:desktop.find(process.pid,app),app+' window missing')
            wait_for(process,lambda:app+' ready:' in content(),app+' packaged/content startup missing')
            if packaged and hz.SYSTEM=='linux' and str(hz.ROOT) in (Path('/proc')/str(process.pid)/'maps').read_text():
                raise RuntimeError('Game package loaded checkout libraries')
            desktop.activate(window)
            def painted():
                width,height,pixels=desktop.capture(window)
                stride=max(1,width*height//2048)*3
                return len({pixels[i:i+3] for i in range(0,len(pixels)-2,stride)})>1
            for width,height in ((960,640),(720,480)):
                desktop.resize(window,width,height)
                wait_for(process,lambda:desktop.geometry(window)[2:]==(width,height) and painted(),app+' did not present after resize',20)
            desktop.capture(window,shots/f'{game}-{app}-startup.png')
            desktop.close(window)
            if process.wait(timeout=30)!=0:raise RuntimeError(app+' failed graceful close')
        except Exception:
            if window and process.poll() is None:
                try:desktop.capture(window,shots/f'{game}-{app}-failure.png')
                except Exception:pass
            raise
        finally:
            if process.poll() is None:process.kill();process.wait()
    text=content()
    if '[error]' in text.lower() or '[critical]' in text.lower() or 'Exception' in text or 'Failed to' in text:
        raise RuntimeError('Game startup/render/lifecycle error:\n'+text)
    if any(hashlib.sha256(p.read_bytes()).hexdigest()!=digest for p,digest in before.items()):
        raise RuntimeError('Smoke changed authored project/scene files')
    print(f'PASS: {game}/{app}, startup/render/resize/shutdown; '+('relocation' if packaged else 'authored files unchanged'),flush=True)


def game_tests(configuration,profile,packages,output):
    logs=hz.ROOT/'build/testing/games'/f'{configuration}-{profile}'
    logs.mkdir(parents=True,exist_ok=True);shots=logs/'screenshots';shots.mkdir(exist_ok=True)
    hz.yaml_tools() # Required by canonical project/build services.
    if not packages:
        for game in GAMES:hz.script_build(hz.ROOT/'examples'/game/(game+'.hproj'),configuration)
    model_tests(configuration)
    driver=software_driver() if hz.SYSTEM=='windows' and profile!='native' else None
    with tempfile.TemporaryDirectory(prefix='Hazel games space-é-🚀 ') as temporary:
        working=Path(temporary);env=graphics_environment(profile)
        native=hz.binaries(configuration)/'MigrationExampleGamesSmoke'/('MigrationExampleGamesSmoke.exe' if hz.SYSTEM=='windows' else 'MigrationExampleGamesSmoke')
        if not native.is_file():raise RuntimeError('Build --tests before test-games')
        if driver:
            private=working/'native executable';private.mkdir();hz.copy_changed(native,private/native.name);native=private/native.name
            for dll in driver.glob('*.dll'):hz.copy_changed(dll,private/dll.name)
        if not packages:
            env.update(HAZEL_RESOURCES=str(hz.binaries(configuration)/'Nutella/Resources'),HAZEL_MONO=str(hz.binaries(configuration)/'Nutella/mono'),HAZEL_DATA=str(working/'native data'))
            if hz.SYSTEM=='linux':env['LD_LIBRARY_PATH']=str(hz.mono_prefix()/'lib')
            with (logs/'RuntimeSmoke.log').open('w',encoding='utf-8') as stream:
                result=subprocess.run([str(native),*[str(hz.ROOT/'examples'/game/(game+'.hproj')) for game in NATIVE_GAMES],str(shots),str(hz.ROOT/'examples/LastLightkeeper/LastLightkeeper.hproj')],cwd=working,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=120)
            if result.returncode:raise RuntimeError(f'Game runtime regression (exit {result.returncode}):\n'+(logs/'RuntimeSmoke.log').read_text(errors='replace'))
            for capture in shots.glob('*.ppm'):ppm_to_png(capture)
        roots=[]
        if packages:
            suffix='.zip' if hz.SYSTEM=='windows' else '.tar.gz'
            for game in GAMES:roots.append(extract_verified(output/f'{game}-{hz.SYSTEM}-x86_64-Release{suffix}',working))
        env=graphics_environment(profile);desktop=Desktop()
        try:
            with unavailable_sources() if packages else contextlib.nullcontext():
                if packages:
                    # Run the same strict gameplay/lifecycle assertions against
                    # extracted projects, shipped Resources/Mono and no source SDK.
                    probe=roots[0]/native.name;hz.copy_changed(native,probe)
                    if driver:
                        for dll in driver.glob('*.dll'):hz.copy_changed(dll,roots[0]/dll.name)
                    isolated=env.copy()
                    isolated.update(HAZEL_RESOURCES=str(roots[0]/'Resources'),HAZEL_MONO=str(roots[0]/'mono'),HAZEL_DATA=str(working/'packaged native data'))
                    with (logs/'PackagedRuntimeSmoke.log').open('w',encoding='utf-8') as stream:
                        result=subprocess.run([str(probe),*[str(next(root.glob('*.hproj'))) for root in roots[:2]],str(shots),str(next(roots[2].glob('*.hproj')))],cwd=working,env=isolated,stdout=stream,stderr=subprocess.STDOUT,timeout=120)
                    if result.returncode:raise RuntimeError('Extracted game runtime regression:\n'+(logs/'PackagedRuntimeSmoke.log').read_text(errors='replace'))
                    for capture in shots.glob('*.ppm'):ppm_to_png(capture)
                for index,game in enumerate(GAMES):
                    for app in (('Nutella',) if packages else ('Nutella','Hazelnut')):
                        directory=roots[index] if packages else hz.binaries(configuration)/app
                        if driver and not packages:
                            private=working/'applications'/app;hz.copy_tree(directory,private);directory=private
                        executable=directory/(app+('.exe' if hz.SYSTEM=='windows' else ''))
                        if driver:
                            for dll in driver.glob('*.dll'):hz.copy_changed(dll,directory/dll.name)
                        project=next(directory.glob('*.hproj')) if packages else hz.ROOT/'examples'/game/(game+'.hproj')
                        env['HAZEL_DATA']=str(working/'data'/game/app)
                        env['HAZEL_SAVE_ROOT']=str(working/'player saves'/game/app)
                        if packages:cli(executable,working,env)
                        exercise(desktop,executable,project,app,game,working,env,logs,shots,packages)
        finally:desktop.shutdown()
