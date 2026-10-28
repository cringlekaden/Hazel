#!/usr/bin/env python3
"""Historical pre-runtime-UI composition; see Authoring/README.md.
Reproduction is restricted to disposable copies. Edit current content in Hazelnut.
"""
import argparse, math, yaml
from compose import historical_arguments, Scene, terrain, quarter, game_hud, menu, pine, WHITE, AMBER, CLIPS, ASSETS
from observatory import masonry_collisions
STONE=[.79,.74,.65,1]
IRON=[.68,.74,.79,1]

def player_hud(s,x,y,home=False):
    player=s.entity('Iona',x,y,.55,.95,.95);s.solid(player,dynamic=True)
    del player['BoxCollider2DComponent']
    player['CircleCollider2DComponent']={'Offset':[0,0],'Radius':.26,'Density':1,'Friction':0,'Restitution':0,'RestitutionThreshold':.5}
    s.sprite('IonaCoat','tiny-dungeon',85,0,0,.02,parent=player['Entity'])
    lamp=s.sprite('IonaLantern','tiny-dungeon',125,.28,-.18,.06,.5,.5,parent=player['Entity'])
    lamp['SpriteAnimationComponent']={'DefaultClip':{'Sheet':'Art/tiny-dungeon.hsprites','ClipID':CLIPS['tiny-dungeon']},'Autoplay':True,'Speed':1}
    camera=s.camera(max(-12,min(12,x)),y,True,player);game_hud(s,camera['Entity'])
    fields={n:s.by_name[n] for n in ['Objective','Context','Place','Inventory','TideLabel','Dialogue','DialoguePanel','SaveLabel','SoundTurn','SoundRestore','SoundHazard','SoundWarning']}
    fields.update(Player=player,Home=home,BeamAsset='Prefabs/LightBeam.hprefab',EndAsset='Prefabs/BeamEnd.hprefab')
    s.script(s.entity('Final chapter controller'),'FinalChapter',fields)

def tower():
    s=Scene('UpperTower');bg=s.image('Sea far below the tower','water-tile.png',0,0,-3,64,64,[.35,.44,.58,1]);bg['SpriteRendererComponent']['Source']['TilingFactor']=64
    floor=s.entity('Tower floor and exposed western gantry',z=-.7)['Entity']
    paved=lambda x,y:-18<=x<=16 and -8<=y<=12
    # Native quarter adjacency, with intact quiet stone in interior cells.
    for y in range(-8,13):
        for x in range(-18,17):
            edge=any(not paved(x+a,y+b) for a,b in [(1,0),(-1,0),(0,1),(0,-1)])
            if not edge:s.sprite('Gallery floor '+str((x,y)),'tiny-town',109,x,y,0,color=STONE,parent=floor);continue
            for dx,dy in [(-1,1),(1,1),(-1,-1),(1,-1)]:
                h=not paved(x+dx,y);v=not paved(x,y+dy)
                tile=((96 if dx<0 else 98) if dy>0 else (120 if dx<0 else 122)) if h and v else (108 if dx<0 else 110) if h else (97 if dy>0 else 121) if v else 109
                s.sprite('Gallery floor %d,%d quarter %d,%d'%(x,y,dx,dy),'ground-quarters',quarter('ground-quarters',tile,0 if dx<0 else 1,0 if dy>0 else 1),x+dx*.25,y+dy*.25,0,.5,.5,STONE,parent=floor)
    stone=0
    def wall(x,y,tile=2):
        nonlocal stone
        e=s.sprite('OpticalStone'+str(stone),'tiny-dungeon',tile,x,y,.35,color=STONE,solid=True);stone+=1;return e
    for x in range(-19,18):
        wall(x,-9,26);wall(x,13,2)
    for y in range(-8,13):wall(-19,y,13);wall(17,y,15)
    for x in range(-18,17):
        if x==-8:s.sprite('EntryGate','tiny-dungeon',47,x,0,.4,color=IRON,solid=True)
        elif x==-16:s.sprite('Entry optical window','tiny-dungeon',61,x,0,.35,color=WHITE,solid=True)
        else:wall(x,0)
    def cabinet(name,x,y,opening):
        for dx in (-1,0,1):
            for dy in (-1,0,1):
                if dx==dy==0:continue
                if (dx,dy)==opening:s.sprite(name+' glass','tiny-dungeon',61,x+dx,y+dy,.35,color=WHITE,solid=True)
                else:wall(x+dx,y+dy,2 if dy==1 else 26 if dy==-1 else 13 if dx==-1 else 15)
        s.sprite(name,'tiny-factory',98,x,y,.5,color=AMBER)
    cabinet('EntryReceiver',-10,2,(-1,0))
    mirror=s.sprite('EntryMirror','tiny-dungeon',61,-16,2,.4,color=WHITE)
    mirror['TransformComponent']['Rotation'][2]=math.pi/4
    cabinet('Contact0',-2,11,(0,-1));cabinet('Contact1',13,5,(-1,0));cabinet('Contact2',6,2,(0,1))
    for name,x,y,angle,label in [('EntryPad',-16,-3,math.pi/2,'DOOR LATCH'),('Aim0',-2,5,math.pi/2,'NORTH CONTACT'),('Aim1',6,5,0,'EAST CONTACT'),('Aim2',6,9,-math.pi/2,'SOUTH CONTACT')]:
        s.sprite(name,'tiny-battle',61,x,y,-.05,color=WHITE)
        e=s.sprite(name+' arrow','tiny-battle',43,x+math.cos(angle)*.7,y+math.sin(angle)*.7,-.04,.45,.45,WHITE);e['TransformComponent']['Rotation'][2]=angle
        s.text(name+' plate',label,x-1.3,y-1.2,.18,AMBER,z=.6)
    for x,y in [(1,7),(1,4),(9,8)]:
        wall(x,y,6)
        s.sprite('Iron column cap '+str((x,y)),'tiny-factory',94,x,y,.5,color=IRON)
    # The north lantern bay opens only after all three separate contacts latch.
    for x in range(9,15):wall(x,10,26);wall(x,13,2)
    for y in (11,12):
        wall(14,y,15)
        if y==11:s.sprite('MainGate','tiny-dungeon',47,9,y,.4,color=IRON,solid=True)
        else:wall(9,y,13)
    for x in (10,11,12):s.sprite('Main lantern drive '+str(x),'tiny-factory',86 if x==11 else 85 if x==10 else 87,x,12,.45,color=AMBER)
    s.sprite('MainLight','tiny-dungeon',56,11,11,.6,color=WHITE)
    s.sprite('BeaconHalo','tiny-dungeon',125,11,12,.7,.001,.001,AMBER)
    for x in range(-13,-10):
        for y in range(-8,0):s.sprite('Wind'+str((x+13)*8+y+8),'tiny-battle',43,x,y,.3,.001,.001,color=[.91,.94,.91,.85])
    for x in (-14,-10):
        for y in (-7,-4,-1):s.sprite('Gantry safety board '+str((x,y)),'tiny-factory',117,x,y,.05,color=AMBER)
    for x in range(-18,-14):s.sprite('Wind shelter rail '+str(x),'tiny-factory',107,x,-7,.2,color=IRON)
    s.sprite('TowerNote','tiny-town',83,-17,-4,.45,color=WHITE)
    s.text('Gantry plaque','EXPOSED WALK',-14,-8.6,.21,AMBER,z=.6)
    s.sprite('StormWisp','tiny-dungeon',121,6,6,.7,1.2,1.2,[.72,.84,1,1])
    s.sprite('RushMark','tiny-battle',192,6,6,.1,.001,.001,AMBER)
    for i,(x,y) in enumerate([(-17,-6),(-8,2),(-5,9)]):
        s.entity('Checkpoint'+str(i),x,y,.55);s.sprite('RestMarker'+str(i),'tiny-dungeon',63,x-1,y,.4,color=AMBER)
    player_hud(s,-17,-6)
    masonry_collisions(s,('EntryGate','MainGate'))
    return s

def harbor():
    s=Scene('HarborHome');bg=s.image('Morning sea','water-tile.png',0,0,-3,64,64,[.47,.64,.73,1]);bg['SpriteRendererComponent']['Source']['TilingFactor']=64
    root=s.entity('Landing headland',z=-1)['Entity']
    land=lambda x,y:-13<=x<=9 and -4<=y<=8 and not (x>7 and y>5) and not (x<-10 and y<-2)
    terrain(s,root,(-17,14,-12,12),land,tint=[.79,.88,.78,1],collide=False)
    # An actual connected pier and work apron lead to the boat, not scattered props.
    apron=s.entity('Stone landing apron',z=-.4)['Entity']
    paved=lambda x,y:-5<=x<=3 and -4<=y<=5
    for y in range(-4,6):
        for x in range(-5,4):
            if all(paved(x+a,y+b) for a,b in [(1,0),(-1,0),(0,1),(0,-1)]):s.sprite('Landing apron '+str((x,y)),'tiny-town',109,x,y,0,color=STONE,parent=apron);continue
            for dx,dy in [(-1,1),(1,1),(-1,-1),(1,-1)]:
                h=not paved(x+dx,y);v=not paved(x,y+dy)
                tile=((96 if dx<0 else 98) if dy>0 else (120 if dx<0 else 122)) if h and v else (108 if dx<0 else 110) if h else (97 if dy>0 else 121) if v else 109
                s.sprite('Landing apron kerb %d,%d quarter %d,%d'%(x,y,dx,dy),'ground-quarters',quarter('ground-quarters',tile,0 if dx<0 else 1,0 if dy>0 else 1),x+dx*.25,y+dy*.25,0,.5,.5,STONE,parent=apron)
    pier=lambda x,y:-2<=x<=0 and -7<=y<=-3
    for y in range(-7,-2):
        for x in (-2,-1,0):s.sprite('Gallery floor '+str((x,y))+' pier','tiny-dungeon',36 if x==-2 else 38 if x==0 else 37,x,y,-.3,color=IRON)
    for y in range(-8,9):
        for x in range(-13,10):
            if not land(x,y) and not pier(x,y):s.solid(s.entity('Landing water edge '+str((x,y)),x,y))
    for x,y in [(-9,4),(-7,6),(6,6),(7,3),(7,-3)]:
        pine(s,'Landing pine '+str((x,y)),x,y,[.78,.87,.74,1])
        s.solid(s.entity('Landing tree root '+str((x,y)),x,y-.35,sx=.75,sy=.9),(.25,.35))
    for name,tile,x,y in [('Orrin',111,-3,-3),('Nessa',99,-1,-3),('Mara',98,3,1),('Cal',86,-3,3)]:
        e=s.sprite(name,'tiny-dungeon',tile,x,y,.6,.95,.95);s.solid(e,(.27,.26))
    for x,y,tile in [(4,3,73),(5,3,74),(6,3,75),(-4,5,63),(-3,5,73),(-2,5,75)]:s.sprite('Landing workbench '+str((x,y)),'tiny-dungeon',tile,x,y,.3,color=STONE,solid=True)
    s.sprite('RepairBook','tiny-town',83,5,2,.45,color=AMBER)
    s.sprite('Cal lamp','tiny-dungeon',125,-3,4,.45,.5,.5,AMBER)
    s.sprite('BoatExit','tiny-town',106,-1,-6,.4,color=WHITE)
    s.text('Landing mooring','ORRIN\'S MOORING',-3,-7.5,.23,AMBER,z=.6)
    s.entity('Checkpoint0',0,0,.55)
    for name,x,y,w,h in [('West',-14,0,1,20),('East',10,0,1,20),('North',0,9,30,1),('South',0,-9,30,1)]:s.solid(s.entity('Landing bound '+name,x,y,sx=w,sy=h))
    player_hud(s,0,0,True);masonry_collisions(s)
    return s

if __name__=='__main__':
    args=historical_arguments()
    scenes=[tower(),harbor(),menu('Ending',True),menu('Credits')]
    scenes[2].by_name['Chapter']['TextComponent']['TextString']='THE MORNING BOAT'
    credits=scenes[3];credits.by_name['Chapter']['TextComponent']['TextString']='THE LAST LIGHTKEEPER'
    for field in credits.by_name['Title / Journey menu']['ScriptComponent']['ScriptFields']:
        if field['Name']=='Ending':field['Name']='CreditsPage';field['Data']=True
    for s in scenes:
        path=ASSETS/'Scenes'/(s.name+'.hazel')
        if path.exists() and not args.overwrite:raise SystemExit('Refusing to replace edited scene: '+str(path))
        path.write_text(yaml.dump({'Scene':s.name,'SceneVersion':2,'Entities':s.entities},Dumper=yaml.CSafeDumper,sort_keys=False,allow_unicode=True),encoding='utf-8')
        print('Authored',s.name,len(s.entities),'editable entities.')
