"""Check real authored collision closure; no gameplay bot or pacing assertion."""
from collections import deque
import math
import re
import yaml


def check_project(project):
    assets=project.parent/'Assets'
    loader=getattr(yaml,'CSafeLoader',yaml.SafeLoader)
    sheet=yaml.load((assets/'Art/tiny-town.hsprites').read_text(encoding='utf-8'),Loader=loader)['SpriteSheet']
    pine=next(r for r in sheet['Regions'] if r['Name']=='pine_tall')
    assert pine['ID']=='be7a5bc884ec131c' and pine['Rect']==[64,0,16,32] and pine['Pivot']==[.5,.5], 'Full native pine identity/crop changed'
    assert sheet['Sampling']['MagFilter']=='Nearest' and not sheet['Sampling']['GenerateMips'], 'Pixel art sampling changed'
    for name,count in {'MainMenu':5,'Dawn':5,'CliffDawn':5,'Breakwater':13,'CliffCistern':3}.items():
        document=yaml.load((assets/'Scenes'/(name+'.hazel')).read_text(encoding='utf-8'),Loader=loader)
        groups={}
        for e in document['Entities']:groups.setdefault(e['Relationship']['Parent'],[]).append(e['Relationship']['Order'])
        assert all(sorted(orders)==list(range(len(orders))) for orders in groups.values()), (name,'Noncontiguous authored hierarchy after tree edits')
        trees=[e for e in document['Entities'] if 'pine' in e['TagComponent']['Tag'].lower()]
        assert len(trees)==count,(name,'Duplicate or missing pine pieces')
        for e in trees:
            source=e['SpriteRendererComponent']['Source'];scale=e['TransformComponent']['Scale']
            assert not e['TagComponent']['Tag'].endswith(' trunk'), 'Pine already includes its base'
            assert source['Sheet']=='Art/tiny-town.hsprites' and source['RegionID']==pine['ID']
            assert scale[:2]==[1,2] and scale[1]/scale[0]==pine['Rect'][3]/pine['Rect'][2], (name,'Pine aspect or native pixel scale changed')
    print('PASS: complete native 16x32 pines, one sprite each, uniform pixel scale and no duplicated bases',flush=True)
    for scene in ('Breakwater','CliffCistern','SeaCaves'):
        document=yaml.load((assets/'Scenes'/(scene+'.hazel')).read_text(encoding='utf-8'),Loader=loader)
        entities=document['Entities'];by_name={e['TagComponent']['Tag']:e for e in entities}
        walk=set()
        for name in by_name:
            if name.startswith(('Bank (','Gravel ','Gallery floor (')):
                values=re.findall(r'-?\d+',name)
                walk.add(tuple(map(int,values[:2])))
        player=by_name['Iona'];scale=player['TransformComponent']['Scale'];size=player['BoxCollider2DComponent']['Size']
        radius=(scale[0]*size[0],scale[1]*size[1])

        def position(name):return tuple(by_name[name]['TransformComponent']['Translation'][:2])
        def reachable(start,removed=(),notches=None,reunited=False):
            obstacles=[]
            for name,e in by_name.items():
                if name in removed or 'BoxCollider2DComponent' not in e or e.get('Rigidbody2DComponent',{}).get('BodyType')!='Static':continue
                assert e['Relationship']['Parent']==0,'Physics authoring owner must remain a root'
                t=e['TransformComponent'];x,y=t['Translation'][:2]
                match=re.fullmatch(r'Shutter(\d)Slat(\d+)',name)
                if match:
                    slot,index=map(int,match.groups());gap=(notches or {}).get(slot,{-1:0,0:-3,1:0,2:0}[slot]);y=index-8+(index-8>=gap)
                    if reunited and ((slot==1 and y in (-7,-6)) or (slot==2 and y==-3)):continue
                collider=e['BoxCollider2DComponent'];sx,sy=collider['Size'];ox,oy=collider['Offset'];wx,wy=t['Scale'][:2]
                obstacles.append((x+ox*wx,y+oy*wy,abs(sx*wx)+radius[0],abs(sy*wy)+radius[1]))
            clear={cell for cell in walk if not any(abs(cell[0]-x)<sx and abs(cell[1]-y)<sy for x,y,sx,sy in obstacles)}
            seed=tuple(round(v) for v in start);assert seed in clear,(scene,'blocked recovery point',seed)
            seen={seed};queue=deque([seed])
            while queue:
                x,y=queue.popleft()
                for cell in ((x+1,y),(x-1,y),(x,y+1),(x,y-1)):
                    if cell in clear and cell not in seen:seen.add(cell);queue.append(cell)
            return seen
        def near(cells,name,distance=1.55):
            x,y=position(name);return any(math.hypot(a-x,b-y)<distance for a,b in cells)
        if scene=='Breakwater':
            start=position('Checkpoint0');initial=reachable(start)
            assert near(initial,'HarborMirror') and not near(initial,'Wrench'), 'Harbor gate must separate the grove'
            opened=reachable(start,('HarborGate',))
            for item in ('Wrench','Pump','Lens','Socket','MirrorOne','MirrorTwo'):assert near(opened,item),(scene,'unreachable landmark',item)
            assert not near(opened,'MirrorThree'),'Service room can be entered before its latch'
            assert near(reachable(start,('HarborGate','ServiceDoor')),'MirrorThree'),'Service door did not expose last mirror'
        elif scene=='CliffCistern':
            start=position('Checkpoint0');initial=reachable(start)
            for item in ('LanternSleeve','Handle0','FocusPad0'):assert near(initial,item),(scene,'unreachable workshop equipment',item)
            assert not near(initial,'FocusPad1'),'Raised walk bypasses drain latch'
            raised=reachable(start,('DrainGate',),{0:3});assert near(raised,'FocusPad1') and not near(raised,'CaveExit'),'Bridge gate bypass'
            assert near(reachable(start,('DrainGate','CaveEntranceGate'),{0:3}),'CaveExit'),'Cave stairs inaccessible after release'
        else:
            start=position('Checkpoint1');initial=reachable(start)
            for item in ('Handle1','FocusPad2'):assert near(initial,item),(scene,'unreachable gallery equipment',item)
            assert not near(initial,'Checkpoint2'),'Gallery gate bypasses its receiver'
            east=reachable(start,('GalleryGate',),{1:6,2:-3})
            for item in ('Handle2','FocusPad3','Checkpoint2'):assert near(east,item),(scene,'unreachable east equipment',item)
            assert not near(east,'Mara',2.1),'Refuge can be entered before rescue receiver'
            assert near(reachable(start,('GalleryGate','MaraDoor'),{1:6,2:-3}),'Mara',2.1),'Released refuge unreachable'
            home=reachable(position('Mara'),('GalleryGate','MaraDoor','ReturnGate'),{1:6,2:-3},True)
            assert near(home,'DryExit'),'Winch key did not create a dry return route'
        print('PASS: '+scene+' authored collider/gate/recovery/landmark connectivity (no playtime assertion)',flush=True)
