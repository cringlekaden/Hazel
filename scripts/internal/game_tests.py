"""Gameplay invariants and actual editor/player/extracted game acceptance."""
import contextlib
import hashlib
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile
import time
import zlib
import hazel as hz
from internal.package_tests import wait_for,unavailable_sources,extract_verified,graphics_environment,cli
from internal.testing.desktop import Desktop
from internal.tests import software_driver

GAMES=('MeadowRun','Skybound')

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


class GameWindow:
    def __init__(self,desktop,window,editor):self.desktop=desktop;self.window=window;self.editor=editor;self.expected=(-6,-3)
    def area(self):return (372,58,570,394) if self.editor else (0,0,*self.desktop.geometry(self.window)[2:])
    def world_click(self,x,y):
        left,top,width,height=self.area();size=max(12,16*height/width)
        self.desktop.click(self.window,left+width/2+x*height/size,top+height/2-y*height/size)
    def explorer(self,strict=True):
        width,height,pixels=self.desktop.capture(self.window)
        left,top,vw,vh=self.area();hits=[];size=max(12,16*vh/vw)
        ex,ey=self.expected;cx=left+vw/2+ex*vh/size;cy=top+vh/2-ey*vh/size;radius=.8*vh/size
        for y in range(max(top,int(cy-radius)),min(top+vh,int(cy+radius)+1)):
            for x in range(max(left,int(cx-radius)),min(left+vw,int(cx+radius)+1)):
                offset=(y*width+x)*3;r,g,b=pixels[offset:offset+3]
                # Linear sampling changes small sprites' edge colors in the editor.
                if b>=81 and abs(r-224)+abs(g-152)+abs(b-91)<=24:hits.append((x,y))
        if len(hits)<3:
            if not strict:return None
            raise RuntimeError('Rendered explorer coat not found near '+str(self.expected))
        x=sum(p[0] for p in hits)/len(hits);y=sum(p[1] for p in hits)/len(hits)
        self.expected=((x-left-vw/2)*size/vh,-(y-top-vh/2)*size/vh)
        return self.expected
    def wisp_ready(self):
        width,height,pixels=self.desktop.capture(self.window);left,top,vw,vh=self.area();size=max(12,16*vh/vw)
        cx=left+vw/2-3*vh/size;cy=top+vh/2;radius=.6*vh/size
        for y in range(max(top,int(cy-radius)),min(top+vh,int(cy+radius)+1)):
            for x in range(max(left,int(cx-radius)),min(left+vw,int(cx+radius)+1)):
                offset=(y*width+x)*3;r,g,b=pixels[offset:offset+3]
                if abs(r-141)<20 and abs(g-216)<20 and abs(b-208)<20:return True
        return False
    def move(self,x,y,until=lambda:False):
        # Feedback from the actual framebuffer, no production testing hooks.
        self.desktop.activate(self.window)
        for step in range(60):
            if until():return
            px,py=self.explorer();dx=x-px;dy=y-py
            if abs(dx)<.22 and abs(dy)<.22:return
            horizontal=abs(dx)>abs(dy);distance=dx if horizontal else dy
            key=ord(('D' if distance>0 else 'A') if horizontal else ('W' if distance>0 else 'S'))
            duration=min(.55,max(.035,abs(distance)/3.4))
            self.expected=(px+(1 if distance>0 else -1)*duration*3.4,py) if horizontal else (px,py+(1 if distance>0 else -1)*duration*3.4)
            self.desktop.set_key(self.window,key,True)
            try:time.sleep(duration)
            finally:self.desktop.set_key(self.window,key,False)
            time.sleep(.08)
        raise RuntimeError('Explorer could not reach authored waypoint '+str((x,y)))


def exercise(desktop,executable,project,app,game,working,env,logs,shots,packaged=False):
    log=logs/f'{game}-{app}.log';before={p:hashlib.sha256(p.read_bytes()).hexdigest() for p in project.parent.glob('Assets/Scenes/*.hazel')}
    command=[str(executable)]
    if not packaged:command+=(['--project'] if app=='Nutella' else [])+[str(project)]
    with log.open('w',encoding='utf-8') as stream:
        process=subprocess.Popen(command,cwd=working,env=env,stdout=stream,stderr=subprocess.STDOUT)
        def content():return log.read_text(errors='replace')
        def expect(text,count=1):return wait_for(process,lambda:content().count(text)>=count,text+' missing in '+game+'/'+app,45)
        try:
            window=wait_for(process,lambda:desktop.find(process.pid,app),app+' window missing')
            desktop.resize(window,1280,720);expect(app+' ready:');time.sleep(.6)
            if packaged and hz.SYSTEM=='linux' and str(hz.ROOT) in (Path('/proc')/str(process.pid)/'maps').read_text():raise RuntimeError('Game package loaded checkout libraries')
            game_window=GameWindow(desktop,window,app=='Hazelnut')
            if app=='Hazelnut':desktop.click(window,657,40);time.sleep(.5)
            desktop.capture(window,shots/f'{game}-{app}-title.png')
            scene='Meadow' if game=='MeadowRun' else 'Flight';transition='Runtime scene: Scenes/'+scene+'.hazel'
            game_window.world_click(0,-1.35 if game=='MeadowRun' else -1.6);expect(transition)
            wait_for(process,lambda:game_window.explorer(False) if game=='MeadowRun' else game_window.wisp_ready(),'Game transition did not reach the displayed framebuffer',15)
            desktop.capture(window,shots/f'{game}-{app}-play.png')
            if game=='MeadowRun':
                # Real controls finish the authored level, then restart and return.
                for point in [(-5,-3),(-5,2),(0,2),(5,2),(4,2),(4,-2),(4,-3.2),(-4,-3.2),(-4,-2),(-5,-2),(-5,2),(6,2),(6,2.8)]:
                    game_window.move(*point,until=lambda:'Runtime scene: Scenes/Complete.hazel' in content())
                expect('Runtime scene: Scenes/Complete.hazel')
                if 'MeadowRun: seed 5' not in content():raise RuntimeError('Completion skipped collectibles')
                desktop.capture(window,shots/f'{game}-{app}-complete.png')
                game_window.world_click(0,-1.4);expect(transition,2)
                if app=='Nutella':desktop.resize(window,900,640)
                escape=0x1b if hz.SYSTEM=='windows' else 0xff1b
                desktop.key(window,escape,.06);expect('Runtime scene: Scenes/MainMenu.hazel')
                game_window.world_click(0,-1.35);expect(transition,3)
                desktop.key(window,ord('R'),.06);expect(transition,4)
                desktop.key(window,escape,.06);expect('Runtime scene: Scenes/MainMenu.hazel',2)
            else:
                # First approachable gate can be passed with regular flaps.
                desktop.activate(window);start=time.monotonic()
                for flap in range(5):
                    while time.monotonic()<start+flap*1.05:time.sleep(.01)
                    desktop.set_key(window,32,True);time.sleep(.05);desktop.set_key(window,32,False)
                expect('Skybound: score 1')
                desktop.capture(window,shots/f'{game}-{app}-flying.png')
                if app=='Nutella':desktop.resize(window,900,640)
                expect('Skybound: game over')
                desktop.capture(window,shots/f'{game}-{app}-over.png')
                game_window.world_click(0,-1);expect(transition,2)
                if app=='Nutella':desktop.resize(window,900,640)
                # A held input causes one flap and one death, never automatic restart.
                desktop.key(window,32,2.5);expect('Skybound: game over',2)
                if content().count(transition)!=2:raise RuntimeError('Held flap restarted the game')
                desktop.key(window,ord('R'),.06);expect(transition,3)
                desktop.key(window,32,.05);expect('Skybound: game over',3)
                game_window.world_click(0,-2.5);expect('Runtime scene: Scenes/MainMenu.hazel')
            if app=='Hazelnut':desktop.click(window,657,40);time.sleep(.3);desktop.capture(window,shots/f'{game}-{app}-stopped.png')
            desktop.close(window)
            if process.wait(timeout=30)!=0:raise RuntimeError('Game application shutdown failed')
        finally:
            if process.poll() is None:process.kill();process.wait()
    text=content()
    if '[error]' in text.lower() or '[critical]' in text.lower() or 'Exception' in text:raise RuntimeError('Gameplay error:\n'+text)
    if any(hashlib.sha256(p.read_bytes()).hexdigest()!=digest for p,digest in before.items()):raise RuntimeError('Play changed authored scene files')
    print(f'PASS: {game}/{app}, actual rendered controls, gameplay, completion/death, restart/menu, '+('relocation' if packaged else 'authoring')+', graceful shutdown',flush=True)


def game_tests(configuration,profile,packages,output):
    logs=hz.ROOT/'build/testing/games'/f'{configuration}-{profile}'
    logs.mkdir(parents=True,exist_ok=True);shots=logs/'screenshots';shots.mkdir(exist_ok=True)
    if not packages:
        hz.yaml_tools()
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
        env.update(HAZEL_RESOURCES=str(hz.binaries(configuration)/'Nutella/Resources'),HAZEL_MONO=str(hz.binaries(configuration)/'Nutella/mono'),HAZEL_DATA=str(working/'native data'))
        if hz.SYSTEM=='linux':env['LD_LIBRARY_PATH']=str(hz.mono_prefix()/'lib')
        with (logs/'RuntimeSmoke.log').open('w',encoding='utf-8') as stream:
            result=subprocess.run([str(native),*[str(hz.ROOT/'examples'/game/(game+'.hproj')) for game in GAMES],str(shots)],cwd=working,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=120)
        if result.returncode:raise RuntimeError('Game runtime regression:\n'+(logs/'RuntimeSmoke.log').read_text(errors='replace'))
        ppm_to_png(shots/'MeadowRun-complete.ppm')
        roots=[]
        if packages:
            suffix='.zip' if hz.SYSTEM=='windows' else '.tar.gz'
            for game in GAMES:roots.append(extract_verified(output/f'{game}-{hz.SYSTEM}-x86_64-Release{suffix}',working))
        env=graphics_environment(profile);desktop=Desktop()
        try:
            with unavailable_sources() if packages else contextlib.nullcontext():
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
                        if packages:cli(executable,working,env)
                        exercise(desktop,executable,project,app,game,working,env,logs,shots,packages)
        finally:desktop.shutdown()
