#!/usr/bin/env python3
"""Initial authored scene composition, never imported/executed by the game.
Native HazelProject grid import creates sheets; this writes normal editable scenes.
Existing scenes require --overwrite explicitly. Subsequent edits belong in Hazelnut.
"""
import argparse, math, json
from pathlib import Path
import yaml
from PIL import Image, ImageDraw
ROOT=Path(__file__).resolve().parents[1]
ASSETS=ROOT/'Assets'
TABLE={}
REGIONS={}
for pack in ('tiny-town','tiny-dungeon','tiny-factory','tiny-battle','panel','coast-quarters','ground-quarters'):
    path=ASSETS/'Art'/(pack+'.hsprites')
    if not path.exists():raise SystemExit('First use the native sheet import commands in README: '+str(path))
    regions=yaml.safe_load(path.read_text())['SpriteSheet']['Regions']
    TABLE[pack]={i:r['ID'] for i,r in enumerate(regions)}
    REGIONS[pack]=regions

CLIPS={pack:yaml.safe_load((ASSETS/'Art'/(pack+'.hsprites')).read_text())['SpriteSheet']['Clips'][0]['ID'] for pack in ['lantern-pulse','tiny-dungeon']}

STORM=[.65,.77,.83,1]
GRASS=[.67,.79,.77,1]
SAND=[.8,.82,.78,1]
SEA=[.31,.47,.63,1]
AMBER=[1,.82,.5,1]
WHITE=[.91,.94,.91,1]

class Scene:
    def __init__(self,name):self.name=name;self.entities=[];self.by_name={};self.orders={}
    def entity(self,name,x=0,y=0,z=0,sx=1,sy=1,parent=0):
        ident=len(self.entities)+1;order=self.orders.get(parent,0);self.orders[parent]=order+1
        e={'Entity':ident,'TagComponent':{'Tag':name},'Relationship':{'Parent':parent,'Order':order},'TransformComponent':{'Translation':[x,y,z],'Rotation':[0,0,0],'Scale':[sx,sy,1]}}
        self.entities.append(e);self.by_name[name]=e;return e
    def sprite(self,name,pack,tile,x,y,z=0,sx=1,sy=1,color=WHITE,parent=0,solid=False):
        e=self.entity(name,x,y,z,sx,sy,parent)
        e['SpriteRendererComponent']={'Color':color,'Source':{'Type':'Region','Sheet':'Art/'+pack+'.hsprites','RegionID':TABLE[pack][tile]}}
        if solid:self.solid(e)
        return e
    def image(self,name,file,x,y,z=0,sx=1,sy=1,color=WHITE,parent=0):
        e=self.entity(name,x,y,z,sx,sy,parent);e['SpriteRendererComponent']={'Color':color,'Source':{'Type':'Texture','Texture':'Art/'+file,'TilingFactor':1}};return e
    def text(self,name,text,x,y,size=.42,color=WHITE,parent=0,z=8):
        e=self.entity(name,x,y,z,size*1.75,size*1.75,parent);e['TextComponent']={'TextString':text,'Color':color,'Kerning':0,'LineSpacing':.15};return e
    def solid(self,e,size=(.5,.5),offset=(0,0),dynamic=False):
        e['Rigidbody2DComponent']={'BodyType':'Dynamic' if dynamic else 'Static','FixedRotation':True,'GravityScale':0}
        e['BoxCollider2DComponent']={'Offset':list(offset),'Size':list(size),'Density':1,'Friction':0,'Restitution':0,'RestitutionThreshold':.5}
    def script(self,e,name,fields={}):
        data=[]
        for key,val in fields.items():
            kind='Entity' if isinstance(val,dict) else 'Bool' if isinstance(val,bool) else 'Int' if isinstance(val,int) else 'Float' if isinstance(val,float) else 'Prefab'
            data.append({'Name':key,'Type':kind,'Data':val['Entity'] if kind=='Entity' else val})
        e['ScriptComponent']={'ClassName':'LastLightkeeper.'+name,'ScriptFields':data}
    def camera(self,x=0,y=0,follow=False,target=None):
        e=self.entity('Camera',x,y,0)
        e['CameraComponent']={'Camera':{'ProjectionType':1,'PerspectiveFOV':.785398,'PerspectiveNear':.01,'PerspectiveFar':1000,'OrthographicSize':18,'OrthographicNear':-10,'OrthographicFar':10},'Primary':True,'FixedAspectRatio':False}
        f={'Height':18.,'Width':29.,'Follow':follow}
        if target:f['Target']=target
        self.script(e,'View',f);return e
    def panel(self,name,x,y,width,height,parent,z=6):
        e=self.entity(name,x,y,z,width,height,parent);pid=e['Entity']
        # Nine composed native sheet regions; no engine nine-slice system necessary.
        for iy in range(3):
            for ix in range(3):
                bx=.25/width;by=.25/height
                dx=[-.5+bx/2,0,.5-bx/2][ix];dy=[.5-by/2,0,-.5+by/2][iy]
                w=[bx,1-2*bx,bx][ix];h=[by,1-2*by,by][iy]
                tile=[0,1,3,4,5,7,12,13,15][iy*3+ix]
                self.sprite(name+' / frame '+str(iy*3+ix),'panel',tile,dx,dy,0,w,h,[.53,.62,.70,.96],pid)
        return e
    def save(self,overwrite=False):
        path=ASSETS/'Scenes'/(self.name+'.hazel')
        if path.exists() and not overwrite:raise SystemExit('Refusing to replace edited scene; use --overwrite: '+str(path))
        path.write_text(yaml.safe_dump({'Scene':self.name,'SceneVersion':2,'Entities':self.entities},sort_keys=False,allow_unicode=True))

def game_hud(s,cid):
    # 1 world-unit inset; two baselines below the prompt; warning has its own row.
    s.panel('Top HUD',0,7.75,28,1.5,cid)
    s.text('Place','',-13,8,.33,AMBER,cid)
    s.text('Objective','',-13,7.35,.40,WHITE,cid)
    s.panel('Bottom HUD',0,-7.45,28,2,cid)
    s.text('Context','',-13,-7.05,.38,WHITE,cid)
    s.text('Inventory','',-13,-7.7,.30,AMBER,cid)
    s.text('SaveLabel','',-13,-8.2,.28,[.77,.84,.89,1],cid)
    s.text('TideLabel','',2,-8.2,.28,AMBER,cid)
    s.panel('DialoguePanel',0,-3,27,6.8,cid,z=6.5)
    s.text('Dialogue','',-12.5,-.5,.46,WHITE,cid,z=8.5)
    s.text('DialogueFooter','',-12.5,-5.7,.36,AMBER,cid,z=8.5)
    for name,file,gain in [('SoundTurn','click_003.wav',.22),('SoundRestore','confirmation_002.wav',.38),('SoundHazard','error_002.wav',.30),('SoundWarning','open_001.wav',.32)]:
        e=s.entity(name);e['AudioSourceComponent']={'Clip':'Audio/'+file,'Gain':gain,'Loop':False,'PlayOnStart':False}

def pine(s,name,x,y,color=GRASS,parent=0):
    # Existing editor-authored region already contains the entire 16x32 tree.
    tile=next(i for i,r in enumerate(REGIONS['tiny-town']) if r['Name']=='pine_tall')
    region=REGIONS['tiny-town'][tile]
    assert region['Rect']==[64,0,16,32] and region['Pivot']==[.5,.5]
    s.sprite(name,'tiny-town',tile,x,y+.65,.7,1,2,color,parent)

# Terrain is assembled once into the authored scene, using a designed coastline.
def land(x,y):
    if -22<=x<=-3 and -15<=y<=-7:
        return not ((x<-19 and y<-13) or (x>-6 and y<-12))
    if -11<=x<=-9 and -6<=y<=-4:return True
    if -21<=x<=21 and -3<=y<=16:
        return not ((x<-18 and y>12) or (x>18 and y<0) or (x>19 and y>14))
    return False

def quarter(pack,tile,qx,qy):
    columns=18 if pack=='coast-quarters' else 12
    return (tile//columns*2+qy)*(columns*2)+tile%columns*2+qx

def edge_quarter(present,x,y,dx,dy,water=True):
    # present = water for coast; gravel for paths. Texture Y points down.
    horizontal=not present(x+dx,y);vertical=not present(x,y+dy)
    if horizontal and vertical:tile=(18 if dy>0 else 54)+(0 if dx<0 else 2) if water else (12 if dy>0 else 36)+(0 if dx<0 else 2)
    elif horizontal:tile=(36 if dx<0 else 38) if water else (24 if dx<0 else 26)
    elif vertical:tile=(19 if dy>0 else 55) if water else (13 if dy>0 else 37)
    elif not present(x+dx,y+dy):tile=(90 if dy>0 else 93)+(0 if dx<0 else (1 if dy>0 else -1)) if water else ({(-1,1):39,(1,1):40,(-1,-1):41,(1,-1):42}[(dx,dy)])
    else:tile=37 if water else 25
    return quarter('coast-quarters' if water else 'ground-quarters',tile,0 if dx<0 else 1,0 if dy>0 else 1)

def terrain(s,parent,bounds,walkable,gravel=lambda x,y:False,tint=STORM):
    xmin,xmax,ymin,ymax=bounds
    sand=lambda x,y:walkable(x,y) and gravel(x,y) and all(walkable(x+a,y+b) for a,b in [(1,0),(-1,0),(0,1),(0,-1)])
    for y in range(ymin,ymax+1):
        for x in range(xmin,xmax+1):
            water=not walkable(x,y)
            if water and any(walkable(x+a,y+b) for a in (-1,0,1) for b in (-1,0,1)):
                for dx,dy in [(-1,1),(1,1),(-1,-1),(1,-1)]:
                    s.sprite('Coast %d,%d quadrant %d,%d'%(x,y,dx,dy),'coast-quarters',edge_quarter(lambda a,b:not walkable(a,b),x,y,dx,dy),x+dx*.25,y+dy*.25,0,.5,.5,tint,parent)
            elif not water and sand(x,y):
                for dx,dy in [(-1,1),(1,1),(-1,-1),(1,-1)]:
                    s.sprite('Gravel %d,%d quadrant %d,%d'%(x,y,dx,dy),'ground-quarters',edge_quarter(sand,x,y,dx,dy,False),x+dx*.25,y+dy*.25,0,.5,.5,tint,parent)
            else:s.sprite(('Sea ' if water else 'Bank ')+str((x,y)),'tiny-battle',37 if water else 0,x,y,0,color=tint,parent=parent)
            if water and any(walkable(x+a,y+b) for a,b in [(1,0),(-1,0),(0,1),(0,-1)]):s.solid(s.entity('Coast collision '+str((x,y)),x,y))

def island():
    s=Scene('Breakwater');background=s.image('Distant sea','water-tile.png',0,0,-3,64,64,STORM);background['SpriteRendererComponent']['Source']['TilingFactor']=64
    ground=s.entity('ISLAND / Authored coast and paths',z=-1)['Entity']
    def path(x,y):
        return (y<-8 and -20<x<-5 or -17<x<-13 and 2<y<6
            or -11<=x<=-9 and -12<=y<=4 or -10<=x<=18 and 0<=y<=2
            or 6<=x<=8 and 2<=y<=10)
    terrain(s,ground,(-26,26,-19,19),land,path)
    paths=s.entity('PATHS / Joined gravel and working surfaces',z=-.72)['Entity']
    for y in range(2,7):
        for x in range(-17,-13):s.sprite('Pump apron %d,%d'%(x,y),'tiny-factory',0,x,y,0,color=SAND,parent=paths)
    for y in range(6,10):
        for x in range(-18,-13):s.sprite('Causeway %d,%d'%(x,y),'tiny-dungeon',36 if x==-18 else 38 if x==-14 else 37,x,y,0,color=STORM,parent=paths)
    for y in range(8,17):
        for x in range(-5,11):s.sprite('Court floor %d,%d'%(x,y),'tiny-town',109,x,y,0,color=STORM,parent=paths)
    # Shallow channel and an authored drawbridge are the first meaningful route gate.
    for y in (-6,-5,-4):
        for x in (-11,-10,-9):s.sprite('Bridge plank %d,%d'%(x,y),'tiny-dungeon',36 if x==-11 else 38 if x==-9 else 37,x,y,-.45,color=STORM)
    gate=s.sprite('HarborGate','tiny-factory',117,-10,-6,.5,3,1,AMBER,solid=True)
    # Harbor cottage: tile-composed slate roof and stone walls.
    for iy,row in enumerate([[48,49,50],[60,61,62],[76,77,79],[88,78,89]]):
        for ix,tile in enumerate(row):s.sprite('Harbor cottage %d,%d'%(ix,iy),'tiny-town',tile,-8+ix,-9-iy,.15,color=STORM,solid=iy>=2)
    for x,y,tile in [(-21,-8,105),(-20,-8,106),(-21,-12,107),(-20,-12,107),(-6,-9,103),(-5,-9,115)]:s.sprite('Harbor salvage %d,%d'%(x,y),'tiny-town',tile,x,y,.4,color=SAND)
    s.sprite('Landing rope','tiny-town',106,-21,-16,.3,color=STORM)
    for x,y in [(-23,-14),(-23,-15),(-23,-16),(-22,-16)]:s.sprite('Dock %d,%d'%(x,y),'tiny-factory',71,x,y,-.5,color=STORM)
    s.sprite('HarborMirror','tiny-dungeon',61,-15,-11,.35,color=WHITE)
    s.sprite('HarborEmitter','tiny-factory',87,-20,-11,.3,color=AMBER)
    s.sprite('HarborReceiver','tiny-factory',98,-15,-8,.3,color=AMBER)
    s.sprite('Orrin','tiny-dungeon',111,-18,-10,.6,.95,.95)
    s.text('Harbor plaque','HARBOR SIGNAL',-19.5,-7.5,.24,AMBER,z=.6)
    # Grove composition: clusters at path edges, an open central walk, distinct clearing/cache.
    grove=s.entity('SALT GROVE / Trees and wildflowers')['Entity']
    clusters=[(-3,3),(-1,5),(1,5),(3,4),(5,6),(9,4),(12,5),(15,3),(17,5),(19,3),(19,7),(-6,5),(-7,6)]
    for i,(x,y) in enumerate(clusters):
        pine(s,'Grove pine '+str(i),x,y,GRASS,grove)
        # Separate root colliders: visual hierarchy never parents physics owners.
        col=s.entity('Tree trunk '+str(i),x,y-.35,sx=.75,sy=.9);s.solid(col,(.25,.35))
    for x,y in [(-2,3),(-1,3),(0,3),(13,3),(14,3),(15,3)]:s.sprite('Salt flowers %d,%d'%(x,y),'tiny-town',2,x,y,-.4,color=GRASS,parent=grove)
    s.sprite('Wrench','tiny-factory',127,14,2,.5,1.05,1.05,AMBER)
    s.sprite('MaraNote','tiny-town',83,11,2,.4,color=SAND)
    s.sprite('Archive','tiny-dungeon',63,3,5,.5,color=AMBER)
    s.sprite('Cache','tiny-dungeon',56,18,6,.5,color=[.65,.87,1,1])
    s.text('Grove plaque','SALT GROVE',9,-.4,.27,WHITE,z=.5)
    # Pump works: compact machines, pipes and a tide causeway with eastern safety alcove.
    for y in range(3,14):
        for x in (-19,-12):
            if x==-12 and y in (7,8,9):continue
            s.sprite('Works pipe %d,%d'%(x,y),'tiny-factory',93,x,y,.5,color=STORM,solid=True)
    for x,y,tile in [(-18,4,85),(-17,4,86),(-18,5,96),(-17,5,98),(-14,4,94),(-14,5,105),(-16,12,86),(-17,12,87)]:s.sprite('Works mechanism %d,%d'%(x,y),'tiny-factory',tile,x,y,.25,color=STORM)
    for x in range(-19,-11):s.sprite('Works north retaining pipe '+str(x),'tiny-factory',104,x,14,.5,color=STORM,solid=True)
    for y in range(6,11):s.sprite('Alcove retaining wall '+str(y),'tiny-factory',93,-11,y,.5,color=STORM,solid=True)
    s.sprite('Pump','tiny-factory',98,-15,3,.5,color=AMBER)
    s.sprite('Lens','tiny-dungeon',56,-15,12,.5,1.2,1.2,[.74,.9,1,1])
    for y in range(7,11):
        for x in range(-18,-13):
            s.sprite('Surge'+str((y-7)*5+x+18),'tiny-battle',37,x,y,.8,color=[.4,.72,.95,.85])
    for x,y in [(-18,6),(-17,6),(-16,6),(-15,6),(-14,6),(-18,11),(-17,11),(-16,11),(-15,11),(-14,11)]:s.sprite('Tide warning %d,%d'%(x,y),'tiny-factory',117,x,y,.1,color=AMBER)
    s.text('Works plaque','TIDE WORKS',-18,1.5,.28,AMBER,z=.5)
    s.text('Alcove plaque','HIGH GROUND',-12,8,.20,AMBER,z=.5)
    # Main puzzle is an authored discrete court. Paths travel around the beams.
    s.sprite('Socket','tiny-factory',92,-4,10,.45,color=AMBER)
    for i,(x,y) in enumerate([(1,10),(1,15),(7,15)]):
        s.sprite('Mirror base '+str(i),'tiny-factory',94,x,y,.18,color=STORM)
        s.sprite(['MirrorOne','MirrorTwo','MirrorThree'][i],'tiny-dungeon',61,x,y,.5,color=WHITE)
    s.sprite('Receiver','tiny-factory',98,7,11,.45,color=AMBER)
    s.sprite('ServiceReceiver','tiny-factory',98,-4,15,.45,color=AMBER)
    s.text('Service plaque','SERVICE LATCH',-5.4,16,.19,WHITE,z=.6)
    for x in range(5,11):
        s.sprite('Mirror room north '+str(x),'tiny-dungeon',4 if x==5 else 5 if x==10 else 2,x,16,.4,color=STORM,solid=True)
        if x!=7:s.sprite('Mirror room sill '+str(x),'tiny-dungeon',16 if x==5 else 17 if x==10 else 26,x,12,.4,color=STORM,solid=True)
    for y in range(13,16):
        for x in (5,10):s.sprite('Mirror room side %d,%d'%(x,y),'tiny-dungeon',45 if x==5 and y==15 else 13 if x==5 else 15,x,y,.4,color=STORM,solid=True)
    s.sprite('ServiceDoor','tiny-dungeon',47,7,12,.5,color=STORM,solid=True)
    for i,(x,y) in enumerate([(1,8),(4,8),(4,9),(4,10),(4,11)]):s.sprite('Occluder'+str(i),'tiny-dungeon',6,x,y,.5,color=STORM,solid=True)
    s.text('Court plaque','THE MIRROR COURT',-4.5,7.25,.28,AMBER,z=.5)
    # The lower lighthouse is built from existing tower stone and optical machinery.
    for iy,row in enumerate([[96,97,97,97,98],[108,109,125,109,110],[108,124,126,123,110],[120,121,122,121,122]]):
        for ix,tile in enumerate(row):s.sprite('Lighthouse stone %d,%d'%(ix,iy),'tiny-town',tile,14+ix,15-iy,.3,color=STORM,solid=ix in (0,4))
    s.sprite('Lantern mechanism','tiny-factory',86,16,14,.55,1.5,1.5,AMBER)
    halo=s.sprite('BeaconHalo','tiny-dungeon',125,16,14.6,.6,.001,.001,AMBER)
    halo['SpriteAnimationComponent']={'DefaultClip':{'Sheet':'Art/tiny-dungeon.hsprites','ClipID':CLIPS['tiny-dungeon']},'Autoplay':True,'Speed':1}
    s.sprite('Tower','tiny-dungeon',36,16,11,.3,1.4,1.4,STORM)
    s.text('Tower plaque','LOWER LANTERN',13,9,.27,WHITE,z=.6)
    # Bounds limit wandering, while authored ocean/cliff contours remain readable.
    for name,x,y,w,h in [('West',-24,0,1,38),('East',24,0,1,38),('South',0,-18,50,1),('North',0,18,50,1)]:s.solid(s.entity('Island bound / '+name,x,y,sx=w,sy=h))
    player=s.entity('Iona',-18,-12,.55,.9,.9);s.solid(player,(.27,.26),dynamic=True)
    s.image('Iona shadow','ui_0000.png',0,-.28,-.1,.66,.22,[.09,.14,.2,.5],player['Entity'])
    s.sprite('IonaCoat','tiny-dungeon',85,0,0,.02,color=WHITE,parent=player['Entity'])
    lamp=s.sprite('IonaLantern','tiny-dungeon',125,.28,-.18,.06,.5,.5,WHITE,parent=player['Entity'])
    lamp['SpriteAnimationComponent']={'DefaultClip':{'Sheet':'Art/tiny-dungeon.hsprites','ClipID':CLIPS['tiny-dungeon']},'Autoplay':True,'Speed':1}
    for i,(x,y) in enumerate([(-18,-12),(-10,-2),(-15,5)]):s.entity('Checkpoint'+str(i),x,y,.55)
    camera=s.camera(-12,-8,True,player);cid=camera['Entity']
    game_hud(s,cid)
    director=s.entity('Breakwater / Journey controller')
    refs={name:s.by_name[name] for name in ['HarborMirror','HarborGate','ServiceDoor','ServiceReceiver','Pump','Lens','Socket','Receiver','Tower','Wrench','MirrorOne','MirrorTwo','MirrorThree','Objective','Context','Place','Inventory','TideLabel','Dialogue','DialoguePanel','SaveLabel','SoundTurn','SoundRestore','SoundHazard','SoundWarning']}
    refs.update(Player=player,Camera=camera,BeamAsset='Prefabs/LightBeam.hprefab',EndAsset='Prefabs/BeamEnd.hprefab',Speed=3.25,InteractionRadius=1.55)
    s.script(director,'Breakwater',refs)
    return s

def menu(name='MainMenu',ending=False):
    s=Scene(name);camera=s.camera();cid=camera['Entity']
    # Coastal vignette at right; title remains in calm negative space.
    headland=lambda x,y: 6<=x<=17 and -8<=y<=6 and not ((x<8 and y>4) or (x>15 and y<-5))
    coast=s.entity('Menu headland',z=-1.5)['Entity']
    terrain(s,coast,(-22,22,-12,12),headland,tint=[.3,.4,.45,1])
    for iy,row in enumerate([[96,97,97,98],[108,109,125,110],[108,124,126,110],[120,121,121,122]]):
        for ix,t in enumerate(row):s.sprite('Tower vignette %d,%d'%(ix,iy),'tiny-town',t,9+ix*1.3,4-iy*1.3,-.4,1.3,1.3,STORM)
    for x,y in [(8,-2),(14,-1),(15,-3),(8,-4),(12,-5)]:pine(s,'Pine vignette %d,%d'%(x,y),x,y,[.35,.48,.43,1])
    s.sprite('Keeper vignette','tiny-dungeon',85,10,-2,-.1,1.2,1.2)
    halo=s.sprite('Lantern halo','tiny-dungeon',125,10.5,2.6,.2,1.1,1.1,AMBER)
    halo['SpriteAnimationComponent']={'DefaultClip':{'Sheet':'Art/tiny-dungeon.hsprites','ClipID':CLIPS['tiny-dungeon']},'Autoplay':True,'Speed':1}
    for x in range(13,20):s.sprite('Light across sea '+str(x),'tiny-battle',42,x,2.6,-.2,1,.4,WHITE)
    s.text('Chapter','THE BREAKWATER · THE CLIFF CISTERN',-13,7,.31,AMBER,cid)
    heading=s.text('Heading','THE LAST\nLIGHTKEEPER',-13,5.1,1.15,WHITE,cid)
    subtitle=s.text('Subtitle','The reef is dark.\nOrrin is waiting at the landing.',-13,1.8,.38,[.75,.83,.85,1],cid)
    status=s.text('Status','',-13,.2,.36,[.72,.8,.84,1],cid)
    button=s.panel('StartButton',-8.5,-2.5,9,1.3,cid);label=s.text('StartLabel','Enter   Begin',-12,-2.68,.40,WHITE,cid)
    new=s.panel('NewButton',-8.5,-4.3,9,1.3,cid);s.text('NewLabel','N   New game',-12,-4.48,.40,WHITE,cid)
    credits=s.panel('CreditsButton',-8.5,-6.1,9,1.3,cid);s.text('CreditsLabel','C   Credits',-12,-6.28,.40,WHITE,cid)
    s.text('Controls','WASD or arrows: move    E: use    Esc: pause',-13,-8,.35,[.67,.75,.79,1],cid)
    s.script(s.entity('Title / Journey menu'),'Menu',dict(Heading=heading,Subtitle=subtitle,Status=status,StartButton=button,StartLabel=label,NewButton=new,CreditsButton=credits,Ending=ending))
    return s

def art_table():
    code='using Hazel;\nnamespace LastLightkeeper {\n    public static class Art {\n'
    for pack,regions in TABLE.items():
        if pack in ('panel','coast-quarters','ground-quarters'):continue
        code+='        private static readonly ulong[] '+pack.replace('-','_')+'={'+','.join('0x'+v+'UL' for v in regions.values())+'};\n'
    code+='        public static Sprite Region(string pack,int tile) {\n            ulong[] ids;switch(pack){\n'
    for pack in TABLE:
        if pack in ('panel','coast-quarters','ground-quarters'):continue
        code+='                case "'+pack+'":ids='+pack.replace('-','_')+';break;\n'
    code+='                default:throw new System.ArgumentException("Unknown imported sheet");\n            }\n            return new Sprite("Art/"+pack+".hsprites",ids[tile]);\n        }\n    }\n}\n'
    (ASSETS/'Scripts/Source/Art.cs').write_text(code)

def reference(scene):
    # Actual imported pixels arranged exactly like the authored island, no invented sprites.
    out=Image.new('RGBA',(53*16,39*16),(20,26,35,255))
    sheets={pack:Image.open(ASSETS/yaml.safe_load((ASSETS/'Art'/(pack+'.hsprites')).read_text())['SpriteSheet']['Texture']).convert('RGBA') for pack in TABLE if pack!='panel'}
    regions_by_pack={pack:yaml.safe_load((ASSETS/'Art'/(pack+'.hsprites')).read_text())['SpriteSheet']['Regions'] for pack in sheets}
    for e in scene.entities:
        src=e.get('SpriteRendererComponent',{}).get('Source',{})
        if not src:continue
        parent=e['Relationship']['Parent']
        if parent and scene.entities[parent-1]['TagComponent']['Tag']=='Camera':continue
        t=e['TransformComponent'];x,y,z=t['Translation'];w,h=t['Scale'][:2]
        if parent:
            p=scene.entities[parent-1]
            if p['Relationship']['Parent']:continue
            pt=p['TransformComponent']['Translation'];x+=pt[0];y+=pt[1]
        if src['Type']=='Region':
            pack=Path(src['Sheet']).stem
            if pack in ('panel','coast-quarters','ground-quarters'):continue
            regions=regions_by_pack[pack]
            rect=next(r['Rect'] for r in regions if r['ID']==src['RegionID']);rx,ry,rw,rh=rect
            im=sheets[pack].crop((rx,ry,rx+rw,ry+rh))
        else:im=Image.open(ASSETS/src['Texture']).convert('RGBA')
        if w>8 or h>8:continue
        im=im.resize((max(1,int(w*16)),max(1,int(h*16))),Image.Resampling.NEAREST)
        out.alpha_composite(im,(int((x+26)*16-im.width/2),int((19-y)*16-im.height/2)))
    out.convert('RGB').save(ROOT/'Authoring/Island-reference.png')
    contact=Image.new('RGB',(800,480),(25,31,42));d=ImageDraw.Draw(contact)
    chosen={'tiny-town':[0,1,2,4,43,48,76,96,115], 'tiny-dungeon':[36,56,61,85,111,124], 'tiny-factory':[71,85,86,87,94,98,99,117], 'tiny-battle':[18,19,36,37,38,55]}
    for row,(pack,tiles) in enumerate(chosen.items()):
        d.text((20,row*110+12),pack,fill=(240,218,170))
        for col,n in enumerate(tiles):
            sheet=sheets[pack];rx,ry,rw,rh=regions_by_pack[pack][n]['Rect']
            im=sheet.crop((rx,ry,rx+rw,ry+rh));im=im.resize((48,48*rh//rw),Image.Resampling.NEAREST)
            if im.height>48:im=im.resize((48*rw//rh,48),Image.Resampling.NEAREST)
            contact.paste(im,(20+col*80,row*110+35),im);d.text((20+col*80,row*110+87),'tile_%04d'%n,fill='white')
    contact.save(ROOT/'Authoring/Asset-reference.png')

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--write',action='store_true');parser.add_argument('--overwrite',action='store_true');args=parser.parse_args()
    if not args.write:raise SystemExit('Pass --write for initial composition. Existing scenes are protected.')
    game=island()
    for scene in [menu(),game,menu('Dawn',True)]:scene.save(args.overwrite)
    art_table();reference(game)
    # Detached ordinary prefab, validated by native serializer/export service.
    beam=Scene('LightBeam');beam.sprite('Light beam','tiny-battle',42,0,0,.25,1,.45,WHITE)
    end=Scene('BeamEnd');end.sprite('Beam endpoint','tiny-battle',43,0,0,.3,.65,.65,WHITE)
    for prefab in [beam,end]:
        (ASSETS/'Prefabs'/(prefab.name+'.hprefab')).write_text(yaml.safe_dump({'Scene':prefab.name,'SceneVersion':2,'PrefabVersion':2,'PrefabRoot':1,'Entities':prefab.entities},sort_keys=False))
    print('Authored',len(game.entities),'island entities; scenes, prefab, ID table and visual references saved.')
