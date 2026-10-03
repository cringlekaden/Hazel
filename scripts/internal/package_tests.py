"""Verify extracted application archives with source/SDK roots unavailable."""
import contextlib
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
import hazel as hz
from internal.testing.desktop import Desktop
from internal.tests import software_driver


def wait_for(process,condition,message,timeout=90):
    deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        if process.poll() is not None:raise RuntimeError(message+f'; process exited {process.returncode}')
        value=condition()
        if value:return value
        time.sleep(.1)
    raise RuntimeError(message)


def cli(executable,working,env):
    headless=env.copy();headless.pop('DISPLAY',None)
    def invoke(args,success,needle=None):
        result=subprocess.run([str(executable),*args],cwd=working,env=headless,text=True,encoding='utf-8',capture_output=True,timeout=20)
        text=result.stdout+result.stderr
        if (result.returncode==0)!=success or (needle and needle not in text):raise RuntimeError('CLI contract failed: '+str(args)+'\n'+text)
        if 'Creating window' in text:raise RuntimeError('CLI validation initialized graphics')
        return text
    invoke(['--help'],True,'StartScene');invoke(['--list-projects'],True,'.hproj')
    for args in (['--unknown'],['--project'],['--project','--help'],['--help','--list-projects'],['--project','missing.hproj'],['--help','--help'],['--list-projects','--list-projects'],['--project','one.hproj','--project','two.hproj'],['unexpected.hproj']):invoke(args,False)
    root=executable.parent;original=next(root.glob('*.hproj'));saved=original.with_suffix('.saved')
    original.rename(saved)
    try:
        invoke([],False,'No root-level .hproj')
        if invoke(['--list-projects'],True).strip():raise RuntimeError('Empty discovery returned a candidate')
    finally:saved.rename(original)
    other=root/'AAA-é.hproj';shutil.copyfile(original,other)
    try:
        candidates=invoke(['--list-projects'],True).splitlines()
        if candidates!=sorted(candidates):raise RuntimeError('Project discovery order is not deterministic')
        invoke([],False,'Multiple projects')
    finally:other.unlink()
    # Explicit relative paths resolve in the invocation directory, before graphics.
    relative=working/'relative-invalid.hproj';relative.write_text('invalid descriptor',encoding='utf-8')
    try:
        text=invoke(['--project',relative.name],False,'Cannot parse/open')
        if relative.as_posix() not in text.replace('\\','/'):
            raise RuntimeError('Relative CLI project did not resolve against invocation directory')
    finally:relative.unlink()
    nested=root/'nested candidates';nested.mkdir()
    try:
        shutil.copyfile(original,nested/'Nested.hproj')
        if invoke(['--list-projects'],True).splitlines()!=[original.name]:raise RuntimeError('Discovery searched below package root')
    finally:shutil.rmtree(nested)


@contextlib.contextmanager
def unavailable_sources():
    # Rename only task-owned resource/SDK roots, always restoring in finally. The
    # canonical build lock excludes builds and VS Code tasks during this check.
    roots=[hz.ROOT/'Hazel/Resources',hz.ROOT/'Hazelnut/Resources',hz.ROOT/'examples',hz.ROOT/'Hazelnut/mono',hz.ROOT/'build/dependencies/mono']
    moved=[]
    try:
        for root in roots:
            if root.exists():
                hidden=root.with_name(root.name+'.package-test-hidden')
                if hidden.exists():raise RuntimeError('Preserve unexpected package-test backup: '+str(hidden))
                root.rename(hidden);moved.append((root,hidden))
        yield
    finally:
        for root,hidden in reversed(moved):hidden.rename(root)


def package_tests(output,profile='native'):
    suffix='.zip' if hz.SYSTEM=='windows' else '.tar.gz'
    archives=[output/f'{name}-{hz.SYSTEM}-x86_64-Release{suffix}' for name in ('Nutella','Hazelnut')]
    logs=hz.ROOT/'build/testing/packages';logs.mkdir(parents=True,exist_ok=True)
    driver=software_driver() if hz.SYSTEM=='windows' and profile!='native' else None
    with tempfile.TemporaryDirectory(prefix='Hazel extraction space-é-🚀 ') as temp:
        working=Path(temp);unrelated=working/'unrelated cwd';unrelated.mkdir()
        packages=[]
        for archive in archives:
            checksum=archive.with_name(archive.name+'.sha256').read_text().split()[0]
            if hashlib.sha256(archive.read_bytes()).hexdigest()!=checksum:raise RuntimeError('Archive checksum mismatch')
            shutil.unpack_archive(archive,working)
            path=working/archive.name.removesuffix(suffix)
            for line in (path/'SHA256SUMS').read_text().splitlines():
                digest,relative=line.split('  ',1)
                if hashlib.sha256((path/relative).read_bytes()).hexdigest()!=digest:raise RuntimeError('Extracted file checksum mismatch: '+relative)
            if any(p.is_symlink() for p in path.rglob('*')):raise RuntimeError('Archive contains runtime symlinks')
            if driver:
                for dll in driver.glob('*.dll'):hz.copy_changed(dll,path/dll.name)
            packages.append(path)
        env=os.environ.copy()
        for key in ('HAZEL_RESOURCES','HAZEL_MONO','MONO_PATH','MONO_CFG_DIR','LD_LIBRARY_PATH','LD_PRELOAD','MESA_GL_VERSION_OVERRIDE','MESA_GLSL_VERSION_OVERRIDE','LIBGL_ALWAYS_SOFTWARE','GALLIUM_DRIVER','LP_NUM_THREADS'):env.pop(key,None)
        if profile in ('software','gl41'):
            env.update(LIBGL_ALWAYS_SOFTWARE='1',GALLIUM_DRIVER='llvmpipe',LP_NUM_THREADS='2')
        if profile=='gl41':env.update(MESA_GL_VERSION_OVERRIDE='4.1',MESA_GLSL_VERSION_OVERRIDE='410')
        desktop=Desktop()
        try:
            with unavailable_sources():
                for path in packages:
                    name=path.name.split('-')[0];executable=path/(name+('.exe' if hz.SYSTEM=='windows' else ''))
                    env['HAZEL_DATA']=str(working/'data'/name)
                    if name=='Nutella':cli(executable,unrelated,env)
                    log=logs/(name+'-'+profile+'.log')
                    with log.open('w',encoding='utf-8') as stream:
                        process=subprocess.Popen([str(executable)],cwd=unrelated,env=env,stdout=stream,stderr=subprocess.STDOUT)
                        try:
                            window=wait_for(process,lambda:desktop.find(process.pid,name),name+' did not create a native window')
                            wait_for(process,lambda:name+' ready:' in log.read_text(errors='replace'),name+' did not finish packaged startup')
                            time.sleep(2);desktop.activate(window)
                            if hz.SYSTEM=='linux':
                                maps=Path('/proc')/str(process.pid)/'maps'
                                text=maps.read_text()
                                if str(hz.ROOT) in text:raise RuntimeError('Extracted application loaded a native library from the source checkout')
                            if name=='Hazelnut':
                                # Saved 1280x720 dock layout: toolbar centered in
                                # the viewport column. Same real mouse path as F5.
                                desktop.click(window,657,40)
                                time.sleep(.5)
                            for repeat in range(2):
                                _,_,width,height=desktop.geometry(window)
                                if name=='Nutella':x,y=width/2,height/2;viewheight=height
                                else:x,y=657,255;viewheight=394
                                desktop.click(window,x,y)
                                wait_for(process,lambda:log.read_text(errors='replace').count('Runtime scene: Scenes/Level1.hazel')>repeat,'Play click did not transition in '+name)
                                desktop.key(window,ord('D'),.4)
                                desktop.click(window,x-viewheight*1.75/10,y-viewheight*3.5/10)
                                wait_for(process,lambda:log.read_text(errors='replace').count('Runtime scene: Scenes/MainMenu.hazel')>repeat,'Return click did not transition in '+name)
                                if name=='Nutella' and repeat==0:desktop.resize(window,900,640)
                            if name=='Hazelnut':desktop.click(window,657,40) # Stop restores authored scene.
                            desktop.close(window)
                            if process.wait(timeout=30)!=0:raise RuntimeError(name+' failed shutdown')
                        finally:
                            if process.poll() is None:process.kill();process.wait()
                    text=log.read_text(errors='replace')
                    if '[error]' in text.lower() or '[critical]' in text.lower() or 'Failed to' in text:raise RuntimeError('Packaged runtime failure:\n'+text)
                    if profile in ('software','gl41') and 'llvmpipe' not in text.lower():raise RuntimeError('Packaged test did not use isolated software graphics')
                    if any(unrelated.glob('HazelProfile*.json')):raise RuntimeError('Release execution produced profiling dumps')
                    print('PASS: extracted '+name+', unrelated cwd, spaces/Unicode, real Play/menu clicks twice, graceful close; source assets/SDK unavailable',flush=True)
        finally:desktop.shutdown()
