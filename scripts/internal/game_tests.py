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
    def control_ready(self,y,game):
        # The transition log precedes presentation. Observe a button's flat interior
        # before sending another click; managed callbacks must have a displayed frame.
        width,height,pixels=self.desktop.capture(self.window)
        left,top,vw,vh=self.area();size=max(12,16*vh/vw)
        x=int(left+vw/2+1.6*vh/size);y=int(top+vh/2-y*vh/size)
        actual=pixels[(y*width+x)*3:(y*width+x)*3+3]
        target=(65,105,86) if game=='MeadowRun' else (175,113,143)
        return len(actual)==3 and sum(abs(a-b) for a,b in zip(actual,target))<=15

    def explorer(self,strict=True):
        width,height,pixels=self.desktop.capture(self.window)
        left,top,vw,vh=self.area();hits=[];size=max(12,16*vh/vw)
        ex,ey=self.expected;cx=left+vw/2+ex*vh/size;cy=top+vh/2-ey*vh/size;radius=.8*vh/size
        def scan(x0,y0,x1,y1):
            for y in range(y0,y1):
                for x in range(x0,x1):
                    offset=(y*width+x)*3;r,g,b=pixels[offset:offset+3]
                    # The coat's interior color distinguishes it from orange seeds.
                    if abs(r-224)+abs(g-152)+abs(b-91)<=12:hits.append((x,y))
        scan(max(left,int(cx-radius)),max(top,int(cy-radius)),min(left+vw,int(cx+radius)+1),min(top+vh,int(cy+radius)+1))
        if len(hits)<3:
            # Slow software rendering can retain input beyond a predicted frame.
            # Recover from actual pixels instead of assuming a fixed input latency.
            hits.clear();scan(left,top,left+vw,top+vh)
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


def meadow_route(project,scene_name,goals):
    # Navigation follows authored colliders; gameplay still uses real desktop keys.
    import yaml
    from collections import deque
    content=yaml.safe_load((project.parent/'Assets/Scenes'/(scene_name+'.hazel')).read_text(encoding='utf-8'))
    entities={e['TagComponent']['Tag']:e for e in content['Entities']}
    obstacles=[]
    for entity in content['Entities']:
        if entity['TagComponent']['Tag']=='Explorer' or 'BoxCollider2DComponent' not in entity:continue
        transform=entity['TransformComponent'];box=entity['BoxCollider2DComponent'];x,y=transform['Translation'][:2]
        obstacles.append((x,y,box['Size'][0]*transform['Scale'][0]+.38,box['Size'][1]*transform['Scale'][1]+.38))
    pond=entities['Pond']['TransformComponent']['Translation']
    def free(cell):
        x,y=cell[0]/2,cell[1]/2
        return -7<=x<=7 and -3.5<=y<=3.5 and (x-pond[0])**2+(y-pond[1])**2>1.4**2 and not any(abs(x-ox)<sx and abs(y-oy)<sy for ox,oy,sx,sy in obstacles)
    start=(-12,-6);route=[]
    for tag in goals:
        target=entities[tag]['TransformComponent']['Translation'];end=(round(target[0]*2),round(target[1]*2))
        candidates=[(x,y) for x in range(-14,15) for y in range(-7,8) if free((x,y)) and (x/2-target[0])**2+(y/2-target[1])**2<.5**2]
        if not candidates:raise RuntimeError('No reachable authored objective '+scene_name+'/'+tag)
        end=min(candidates,key=lambda c:(c[0]/2-target[0])**2+(c[1]/2-target[1])**2)
        queue=deque([start]);parents={start:None}
        while queue and end not in parents:
            current=queue.popleft()
            for delta in ((1,0),(-1,0),(0,1),(0,-1)):
                cell=(current[0]+delta[0],current[1]+delta[1])
                if cell not in parents and free(cell):parents[cell]=current;queue.append(cell)
        if end not in parents:raise RuntimeError('Objective blocked by authored layout '+scene_name+'/'+tag)
        path=[];cell=end
        while cell is not None:path.append(cell);cell=parents[cell]
        path.reverse()
        compact=[]
        for i,cell in enumerate(path):
            if i==0:continue
            if i==len(path)-1 or (cell[0]-path[i-1][0],cell[1]-path[i-1][1])!=(path[i+1][0]-cell[0],path[i+1][1]-cell[1]):compact.append((cell[0]/2,cell[1]/2))
        route.extend(compact);start=end
    return route

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
            if app=='Hazelnut':
                last_click=0;stop_frames=0
                def enter_play():
                    nonlocal last_click,stop_frames
                    width,height,pixels=desktop.capture(window)
                    def white(x,y):
                        color=pixels[(y*width+x)*3:(y*width+x)*3+3]
                        return len(color)==3 and min(color)>220
                    # Reject an unpainted white client area; distinguish the square
                    # from the triangle across its interior, over two presented frames.
                    painted=not white(638,39)
                    stop=painted and all(white(x,y) for x in (650,656,662) for y in (33,39,45))
                    stop_frames=stop_frames+1 if stop else 0
                    if stop_frames>=2:return True
                    if painted and not stop and white(657,39) and time.monotonic()-last_click>2:
                        last_click=time.monotonic();desktop.click(window,657,40)
                    return False
                wait_for(process,enter_play,'Editor Play did not reach its rendered Stop state',20)
            def menu_ready():return wait_for(process,lambda:game_window.control_ready(-1.35 if game=='MeadowRun' else -1.6,game),'Title controls were not displayed',15)
            def level_ready():
                game_window.expected=(-6,-3)
                return wait_for(process,lambda:game_window.explorer(False) if game=='MeadowRun' else game_window.wisp_ready(),'Game transition did not reach the displayed framebuffer',15)
            menu_ready()
            desktop.capture(window,shots/f'{game}-{app}-title.png')
            scene='Meadow' if game=='MeadowRun' else 'Flight';transition='Runtime scene: Scenes/'+scene+'.hazel'
            game_window.world_click(0,-1.35 if game=='MeadowRun' else -1.6);expect(transition)
            level_ready()
            desktop.capture(window,shots/f'{game}-{app}-play.png')
            if game=='MeadowRun':
                # Complete all three authored levels with actual key input and framebuffer feedback.
                for level,next_scene in [('Meadow','Orchard'),('Orchard','LanternGrove'),('LanternGrove','Complete')]:
                    goal='Runtime scene: Scenes/'+next_scene+'.hazel'
                    for point in meadow_route(project,level,['Seed'+str(i) for i in range(5)]+['Exit']):
                        game_window.move(*point,until=lambda goal=goal:goal in content())
                    expect(goal)
                    if next_scene!='Complete':
                        level_ready();desktop.capture(window,shots/f'{game}-{app}-{next_scene}.png')
                expect('Runtime scene: Scenes/Complete.hazel')
                if 'MeadowRun: seed 5' not in content():raise RuntimeError('Completion skipped collectibles')
                wait_for(process,lambda:game_window.control_ready(-1.4,game),'Completion controls were not displayed',15)
                desktop.capture(window,shots/f'{game}-{app}-complete.png')
                game_window.world_click(0,-1.4);expect(transition,2);level_ready()
                if app=='Nutella':desktop.resize(window,900,640)
                escape=0x1b if hz.SYSTEM=='windows' else 0xff1b
                desktop.key(window,escape,.06);expect('Runtime scene: Scenes/MainMenu.hazel');menu_ready()
                game_window.world_click(0,-1.35);expect(transition,3);level_ready()
                desktop.key(window,ord('R'),.06);expect(transition,4);level_ready()
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
                wait_for(process,lambda:game_window.control_ready(-1,game),'Game-over controls were not displayed',15)
                desktop.capture(window,shots/f'{game}-{app}-over.png')
                game_window.world_click(0,-1);expect(transition,2);level_ready()
                if app=='Nutella':desktop.resize(window,900,640)
                # A held input causes one flap and one death, never automatic restart.
                desktop.key(window,32,2.5);expect('Skybound: game over',2)
                if content().count(transition)!=2:raise RuntimeError('Held flap restarted the game')
                desktop.key(window,ord('R'),.06);expect(transition,3);level_ready()
                desktop.key(window,32,.05);expect('Skybound: game over',3)
                wait_for(process,lambda:game_window.control_ready(-1,game),'Game-over controls were not displayed',15)
                game_window.world_click(0,-2.5);expect('Runtime scene: Scenes/MainMenu.hazel')
            if app=='Hazelnut':desktop.click(window,657,40);time.sleep(.3);desktop.capture(window,shots/f'{game}-{app}-stopped.png')
            desktop.close(window)
            if process.wait(timeout=30)!=0:raise RuntimeError('Game application shutdown failed')
        except Exception:
            if process.poll() is None:
                try:desktop.capture(window,shots/f'{game}-{app}-failure.png')
                except Exception:pass # Preserve the original failure if the window retired.
            raise
        finally:
            if process.poll() is None:process.kill();process.wait()
    text=content()
    if '[error]' in text.lower() or '[critical]' in text.lower() or 'Exception' in text:raise RuntimeError('Gameplay error:\n'+text)
    if any(hashlib.sha256(p.read_bytes()).hexdigest()!=digest for p,digest in before.items()):raise RuntimeError('Play changed authored scene files')
    print(f'PASS: {game}/{app}, actual rendered controls, gameplay, completion/death, restart/menu, '+('relocation' if packaged else 'authoring')+', graceful shutdown',flush=True)


def game_tests(configuration,profile,packages,output):
    logs=hz.ROOT/'build/testing/games'/f'{configuration}-{profile}'
    logs.mkdir(parents=True,exist_ok=True);shots=logs/'screenshots';shots.mkdir(exist_ok=True)
    hz.yaml_tools() # Route analysis also needs YAML for extracted projects, before SDK directories are parked.
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
        env.update(HAZEL_RESOURCES=str(hz.binaries(configuration)/'Nutella/Resources'),HAZEL_MONO=str(hz.binaries(configuration)/'Nutella/mono'),HAZEL_DATA=str(working/'native data'))
        if hz.SYSTEM=='linux':env['LD_LIBRARY_PATH']=str(hz.mono_prefix()/'lib')
        with (logs/'RuntimeSmoke.log').open('w',encoding='utf-8') as stream:
            result=subprocess.run([str(native),*[str(hz.ROOT/'examples'/game/(game+'.hproj')) for game in GAMES],str(shots)],cwd=working,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=120)
        if result.returncode:raise RuntimeError('Game runtime regression:\n'+(logs/'RuntimeSmoke.log').read_text(errors='replace'))
        for capture in shots.glob('*.ppm'):ppm_to_png(capture)
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
