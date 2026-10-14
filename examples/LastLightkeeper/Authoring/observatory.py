#!/usr/bin/env python3
"""One-time composition of editable native scenes, never run by the game.
Existing scenes require explicit --overwrite; preserved chapters are not touched.
"""
import argparse
import yaml
from compose import Scene, terrain, quarter, game_hud, menu, WHITE, AMBER, CLIPS, ASSETS

SLATE=[.69,.73,.81,1]
COPPER=[.9,.77,.62,1]

def observatory():
    s=Scene('Observatory')
    bg=s.image('Sea below the fallen roof','water-tile.png',0,0,-3,64,64,[.43,.52,.67,1])
    bg['SpriteRendererComponent']['Source']['TilingFactor']=64
    ground=s.entity('Survey hall footprint and collapsed central floor',z=-1)['Entity']
    walk=lambda x,y:-22<=x<=22 and -9<=y<=9 and (y<=-4 or y>=4 or x<=-16 or x>=16)
    terrain(s,ground,(-26,26,-13,13),lambda x,y:-22<=x<=22 and -9<=y<=9,tint=SLATE)
    floor=s.entity('Slate terraces and north instrument room',z=-.7)['Entity']
    paved=lambda x,y:-21<=x<=21 and -8<=y<=8 and walk(x,y)
    for y in range(-9,10):
        for x in range(-21,22):
            if not walk(x,y):
                s.sprite('Flooded survey floor '+str((x,y)),'tiny-battle',37,x,y,-.01,color=SLATE,parent=floor)
                if any(walk(x+a,y+b) for a,b in [(1,0),(-1,0),(0,1),(0,-1)]):s.solid(s.entity('Collapsed floor collision '+str((x,y)),x,y))
                continue
            if not paved(x,y):continue
            # Same quarter adjacency used by the cistern: stone stays stone at
            # convex and concave corners. Grass coasts belong only outside.
            for dx,dy in [(-1,1),(1,1),(-1,-1),(1,-1)]:
                horizontal=not paved(x+dx,y);vertical=not paved(x,y+dy)
                tile=((96 if dx<0 else 98) if dy>0 else (120 if dx<0 else 122)) if horizontal and vertical else (108 if dx<0 else 110) if horizontal else (97 if dy>0 else 121) if vertical else 109
                region=quarter('ground-quarters',tile,0 if dx<0 else 1,0 if dy>0 else 1)
                # Interior cells use a single intact tile, rather than four entities.
                if not any(not paved(x+a,y+b) for a,b in [(1,0),(-1,0),(0,1),(0,-1)]):
                    if dx==-1 and dy==1:s.sprite('Survey floor '+str((x,y)),'tiny-town',109,x,y,0,color=SLATE,parent=floor)
                else:s.sprite('Survey floor edge %d,%d quadrant %d,%d'%(x,y,dx,dy),'ground-quarters',region,x+dx*.25,y+dy*.25,0,.5,.5,SLATE,parent=floor)
    stone=0
    def wall(x,y,tile=2):
        nonlocal stone
        e=s.sprite('OpticalStone'+str(stone),'tiny-dungeon',tile,x,y,.35,color=SLATE,solid=True);stone+=1;return e
    # Authored split at the south approach: powered bridge is the only crossing.
    for y in (-9,-8,-7,-4):
        wall(0,y,13)
    gate=s.entity('SpanGate',0,-5.5,.4);s.solid(gate,(1.5,1))
    bridge=s.entity('BridgeDeck',0,-5.5,-.4,.001,.001)
    for x in (-1,0,1):
        for y in (-.5,.5):
            s.sprite('Span safety board '+str((x,y)),'tiny-factory',117,x,y,0,color=AMBER,parent=gate['Entity'])
            s.sprite('Retracted bridge plank '+str((x,y)),'tiny-dungeon',37,x,y,0,color=SLATE,parent=bridge['Entity'])
    for x in (-2,2):
        for y in (-6,-5):s.sprite('South span deck '+str((x,y)),'tiny-dungeon',36 if x==-2 else 38,x,y,-.35,color=SLATE)
    # The north room is sealed until its independently latched door releases.
    for x in range(-22,23):
        if x==18:continue
        wall(x,4,4 if x==-22 else 5 if x==22 else 2)
    s.sprite('CrewGate','tiny-dungeon',47,18,4,.4,color=SLATE,solid=True)
    for x in range(-22,23):
        if x in (7,8,9):s.sprite('North survey window '+str(x),'tiny-dungeon',61,x,9,.35,color=[.7,.85,1,1],solid=True)
        else:wall(x,9,4 if x==-22 else 5 if x==22 else 2)
    for x in (-22,22):
        for y in range(5,9):wall(x,y,13 if x<0 else 15)
    # Receiver cabinets project into the void. Glass admits light, not walking.
    def cabinet(name,x,y,west_open):
        for dx in (-1,0,1):
            wall(x+dx,y-1,16 if dx==-1 else 17 if dx==1 else 26)
            wall(x+dx,y+1,4 if dx==-1 else 5 if dx==1 else 2)
        wall(x+(1 if west_open else -1),y,15 if west_open else 13)
        s.sprite(name+' glass','tiny-dungeon',61,x+(-1 if west_open else 1),y,.35,1,1,[.7,.85,1,1],solid=True)
        s.sprite(name,'tiny-factory',98,x,y,.45,color=AMBER)
    cabinet('SpanReceiver',13,0,True)
    cabinet('DomeReceiver',-13,2,False)
    cabinet('TowerReceiver',13,7,True)
    # Fallen roof members and carriage rails have a job, rather than random rubble.
    for y in range(-3,4):
        s.sprite('Suspended carriage rail '+str(y),'tiny-factory',107,10,y,-.35,.55,1,SLATE)
    for y in (-2,0,2):
        s.sprite('Carriage cradle '+str(y),'tiny-factory',94,10,y,-.25,color=COPPER)
    s.sprite('SurveyCarriage','tiny-factory',86,10,2,.45,color=COPPER,solid=True)
    s.sprite('CarriageHandle','tiny-factory',79,18,-2,.55,color=AMBER,solid=True)
    for x in range(11,18):s.sprite('Winch cable '+str(x),'tiny-factory',107,x,-2,-.2,1,.3,COPPER)
    for x,y in [(-11,-2),(-10,-2),(4,-2),(5,-2)]:s.sprite('Fallen roof stone '+str((x,y)),'tiny-dungeon',6,x,y,-.3,color=SLATE)
    for name,x,y,left in [('WestPost',-19,0,False),('EastPost',19,2,True),('NorthPost',-19,7,False)]:
        s.sprite(name,'tiny-battle',61,x,y,-.05,color=WHITE)
        arrow=s.sprite(name+' direction','tiny-battle',43,x+(-.7 if left else .7),y,-.04,.45,.45,WHITE)
        if left:arrow['TransformComponent']['Rotation'][2]=3.141592653589793
    # Repair bench: compatible lantern fitting, not a second lighthouse lens.
    for x,y,tile in [(-20,-4,73),(-19,-4,74),(-18,-4,75),(-21,-4,63)]:s.sprite('Lens bench '+str((x,y)),'tiny-dungeon',tile,x,y,.2,color=COPPER,solid=True)
    s.sprite('Collimator','tiny-dungeon',56,-19,-3,.5,color=AMBER)
    s.sprite('SurveyNote','tiny-town',83,-21,-3,.45,color=WHITE)
    s.text('Bench label','LONG-FOCUS LENS',-21,-4.8,.21,AMBER,z=.6)
    s.text('Span label','SPAN DRIVE',11,-1.8,.18,WHITE,z=.6)
    s.text('Dome label','NORTH DOOR',-15,3.8,.18,WHITE,z=.6)
    s.text('Winch label','TOWER WINCH',11,8.8,.18,WHITE,z=.6)
    # North room: two coherent instrument desks and a protected evacuation stair.
    for x,y,tile in [(-7,6,63),(-6,6,73),(-5,6,74),(5,8,63),(6,8,73),(7,8,75),(17,8,87),(18,8,94)]:
        s.sprite('Survey instrument '+str((x,y)),'tiny-dungeon' if tile<80 else 'tiny-factory',tile,x,y,.3,color=COPPER,solid=True)
    nessa=s.sprite('Nessa','tiny-dungeon',99,8,6,.6,.95,.95,WHITE);s.solid(nessa,(.27,.26))
    cal=s.sprite('Cal','tiny-dungeon',86,4,6,.6,.95,.95,WHITE);s.solid(cal,(.27,.26))
    s.sprite('Crew ledger','tiny-town',83,5,6,.4,color=AMBER)
    s.sprite('TowerGate','tiny-dungeon',47,-20,6,.4,color=SLATE,solid=True)
    for y in (5,7,8):wall(-20,y,13)
    s.sprite('TowerExit','tiny-dungeon',36,-21,6,.2,color=WHITE)
    for i,(x,y) in enumerate([(-20,-6),(20,-4),(17,6)]):
        s.entity('Checkpoint'+str(i),x,y,.55)
        s.sprite('RestMarker'+str(i),'tiny-dungeon',63,x,y-1,.4,color=AMBER)
    player=s.entity('Iona',-20,-6,.55,.95,.95);s.solid(player,(.27,.26),dynamic=True)
    # Rounded feet slide past touching masonry fixtures rather than catching
    # polygon corners when the player holds against a wall. Existing engine
    # circle physics, authored and inspectable; no movement/raycast workaround.
    del player['BoxCollider2DComponent']
    player['CircleCollider2DComponent']={'Offset':[0,0],'Radius':.26,'Density':1,'Friction':0,'Restitution':0,'RestitutionThreshold':.5}
    s.sprite('IonaCoat','tiny-dungeon',85,0,0,.02,color=WHITE,parent=player['Entity'])
    lamp=s.sprite('IonaLantern','tiny-dungeon',125,.28,-.18,.06,.5,.5,WHITE,parent=player['Entity'])
    lamp['SpriteAnimationComponent']={'DefaultClip':{'Sheet':'Art/tiny-dungeon.hsprites','ClipID':CLIPS['tiny-dungeon']},'Autoplay':True,'Speed':1}
    camera=s.camera(-12,-3,True,player);game_hud(s,camera['Entity'])
    names=['Objective','Context','Place','Inventory','TideLabel','Dialogue','DialoguePanel','SaveLabel','SoundTurn','SoundRestore','SoundHazard','SoundWarning']
    refs={name:s.by_name[name] for name in names}
    refs.update(Player=player,BeamAsset='Prefabs/LightBeam.hprefab',EndAsset='Prefabs/BeamEnd.hprefab')
    s.script(s.entity('Observatory controller'),'Observatory',refs)
    masonry_collisions(s)
    return s

def masonry_collisions(s, additional_gates=()):
    # Box2D can catch a sliding actor on internal corners between touching tile
    # bodies. Fixed grid masonry/furniture therefore owns continuous authored
    # rectangles. Every visual tile remains independent and editor-selectable;
    # movable/destroyable gates, carriage and small actor fixtures keep ownership.
    occupied=set();obsolete=set()
    for e in s.entities:
        name=e['TagComponent']['Tag'];t=e['TransformComponent'];c=e.get('BoxCollider2DComponent')
        if not c or name in ('SurveyCarriage','CrewGate','TowerGate','SpanGate')+additional_gates:continue
        if e['Rigidbody2DComponent']['BodyType']!='Static' or t['Scale']!=[1,1,1] or c['Size']!=[.5,.5] or c['Offset']!=[0,0]:continue
        x,y=t['Translation'][:2]
        if int(x)!=x or int(y)!=y:continue
        occupied.add((int(x),int(y)));del e['BoxCollider2DComponent'];del e['Rigidbody2DComponent']
        if set(e)=={'Entity','TagComponent','Relationship','TransformComponent'}:obsolete.add(e['Entity'])
    while occupied:
        x,y=min(occupied,key=lambda p:(p[1],p[0]));width=1
        while (x+width,y) in occupied:width+=1
        height=1
        while all((a,y+height) in occupied for a in range(x,x+width)):height+=1
        for b in range(y,y+height):
            for a in range(x,x+width):occupied.remove((a,b))
        collider=s.entity('Fixed solid rectangle %d..%d, %d..%d'%(x,x+width-1,y,y+height-1),x+(width-1)*.5,y+(height-1)*.5)
        s.solid(collider,(width*.5,height*.5))
    # Retain existing UUIDs/references; only obsolete empty collider markers go.
    s.entities=[e for e in s.entities if e['Entity'] not in obsolete]
    s.by_name={name:e for name,e in s.by_name.items() if e['Entity'] not in obsolete}
    s.orders={}
    for e in s.entities:
        p=e['Relationship']['Parent'];e['Relationship']['Order']=s.orders.get(p,0);s.orders[p]=s.orders.get(p,0)+1

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--write',action='store_true');parser.add_argument('--overwrite',action='store_true');args=parser.parse_args()
    if not args.write:raise SystemExit('Pass --write to compose native scenes.')
    scenes=[observatory(),menu('ObservatoryDawn',True)]
    scenes[1].by_name['Chapter']['TextComponent']['TextString']='THE OBSERVATORY'
    for s in scenes:
        path=ASSETS/'Scenes'/(s.name+'.hazel')
        if path.exists() and not args.overwrite:raise SystemExit('Refusing to replace edited scene: '+str(path))
        # LibYAML avoids spending minutes formatting the one-time large document.
        path.write_text(yaml.dump({'Scene':s.name,'SceneVersion':2,'Entities':s.entities},Dumper=yaml.CSafeDumper,sort_keys=False,allow_unicode=True),encoding='utf-8')
        print('Authored',s.name,len(s.entities),'editable entities.')
