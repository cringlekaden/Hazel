#!/usr/bin/env python3
"""Authored chapter composition. Outputs normal, editable Hazel scene documents.
No runtime terrain generation. Use --overwrite only after reviewing scene edits.
"""
import argparse
from compose import Scene,terrain,STORM,WHITE,AMBER,CLIPS,game_hud,pine,quarter

COLD=[.57,.68,.77,1]
CAVE=[.58,.62,.71,1]

def hud(s,caves):
    player=s.entity('Iona',-17,-4,.55,.95,.95);s.solid(player,(.27,.26),dynamic=True)
    s.sprite('IonaCoat','tiny-dungeon',85,0,0,.02,color=WHITE,parent=player['Entity'])
    lamp=s.sprite('IonaLantern','tiny-dungeon',125,.28,-.18,.06,.5,.5,WHITE,parent=player['Entity'])
    lamp['SpriteAnimationComponent']={'DefaultClip':{'Sheet':'Art/tiny-dungeon.hsprites','ClipID':CLIPS['tiny-dungeon']},'Autoplay':True,'Speed':1}
    camera=s.camera(-12,-3,True,player);cid=camera['Entity']
    game_hud(s,cid)
    root=s.entity('Sea caves controller' if caves else 'Cliff cistern controller')
    refs={name:s.by_name[name] for name in ['Iona','Camera','Objective','Context','Place','Inventory','TideLabel','Dialogue','DialoguePanel','SaveLabel','SoundTurn','SoundRestore','SoundHazard','SoundWarning']}
    refs['Player']=refs.pop('Iona');refs.update(Caves=caves,BeamAsset='Prefabs/LightBeam.hprefab',EndAsset='Prefabs/BeamEnd.hprefab')
    s.script(root,'CliffChapter',refs)
    return player

def shutter(s,slot,x,default_gap):
    group=s.entity('Shutter %d — metal carriage on three notches'%slot)['Entity']
    # Physics owners stay roots. Every slat remains inspectable at Edit time.
    for i,y in enumerate(y for y in range(-8,9) if y!=default_gap):
        e=s.sprite('Shutter%dSlat%d'%(slot,i),'tiny-factory',116,x,y,.4,color=COLD,solid=True)
    s.sprite('Handle'+str(slot),'tiny-factory',79,x-2,-4,.55,color=AMBER)
    s.sprite('SlotArrow'+str(slot),'tiny-battle',43,x-.75,default_gap,.5,.65,.65,WHITE)
    for y in (-3,0,3) if slot==0 else (0,3,6) if slot==1 else (-3,0,3):
        s.sprite('Rail notch %d at %d'%(slot,y),'tiny-factory',107,x-1,y,.2,.5,.5,WHITE,parent=group)

def pad(s,name,x,y):
    s.sprite(name,'tiny-battle',61,x,y,-.05,1.1,1.1,WHITE)
    s.sprite(name+' facing','tiny-battle',43,x+.7,y,-.05,.4,.4,WHITE)

def cistern():
    s=Scene('CliffCistern');background=s.image('Distant sea','water-tile.png',0,0,-3,64,64,COLD);background['SpriteRendererComponent']['Source']['TilingFactor']=64
    terrainRoot=s.entity('Cliff plateau — coast adjacency',z=-1)['Entity']
    walk=lambda x,y:-19<=x<=18 and -8<=y<=8 and not (x<-17 and y>5) and not (x>16 and y<-6)
    terrain(s,terrainRoot,(-23,23,-12,12),walk,lambda x,y:-18<=x<=-12 and -6<=y<=-3,tint=COLD)
    floor=s.entity('Cistern pavement — stone and inspection walks',z=-.7)['Entity']
    def paved(x,y):
        return (-11<=x<=1 and -1<=y<=1 or -9<=x<=-7 and 0<=y<=4
            or -11<=x<=-1 and 3<=y<=4 or 1<=x<=12 and -8<=y<=1
            or 1<=x<=9 and 2<=y<=4 or 7<=x<=9 and 3<=y<=7
            or 10<=x<=12 and 5<=y<=7 or 13<=x<=18 and 5<=y<=7)
    for y in range(-8,9):
        for x in range(-11,19):
            if not (walk(x,y) and paved(x,y)):continue
            if all(paved(x+a,y+b) for a,b in [(1,0),(-1,0),(0,1),(0,-1)]):
                s.sprite('Pavement '+str((x,y)),'tiny-town',109,x,y,0,color=COLD,parent=floor)
            else:
                for dx,dy in [(-1,1),(1,1),(-1,-1),(1,-1)]:
                    horizontal=not paved(x+dx,y);vertical=not paved(x,y+dy)
                    tile=((96 if dx<0 else 98) if dy>0 else (120 if dx<0 else 122)) if horizontal and vertical else (108 if dx<0 else 110) if horizontal else (97 if dy>0 else 121) if vertical else 109
                    region=quarter('ground-quarters',tile,0 if dx<0 else 1,0 if dy>0 else 1)
                    s.sprite('Pavement edge %d,%d quadrant %d,%d'%(x,y,dx,dy),'ground-quarters',region,x+dx*.25,y+dy*.25,0,.5,.5,COLD,floor)
    # Pool and kerb are composed from actual water/stone tiles, with aligned collisions.
    for y in range(-6,0):
        for x in range(3,11):s.sprite('Cistern water '+str((x,y)),'tiny-battle',37,x,y,-.5,color=COLD,solid=True)
        for x in (2,11):s.sprite('Pool kerb '+str((x,y)),'tiny-town',108 if x==2 else 110,x,y,-.4,color=COLD,solid=True)
    for x in range(2,12):
        for y in (0,-7):s.sprite('Pool kerb '+str((x,y)),'tiny-town',(96 if x==2 else 98 if x==11 else 97) if y==0 else (120 if x==2 else 122 if x==11 else 121),x,y,-.4,color=COLD,solid=True)
    stone=0
    # Workshop composition: open roofless bench space, wood shelves, tool racks.
    for x in range(-18,-11):s.sprite('Stone'+str(stone),'tiny-town',96 if x==-18 else 98 if x==-12 else 97,x,-2,.2,color=COLD,solid=True);stone+=1
    for x,y,t in [(-17,-5,63),(-16,-5,73),(-15,-3,74),(-13,-3,75)]:s.sprite('Workshop furnishing '+str((x,y)),'tiny-dungeon',t,x,y,.3,color=WHITE)
    s.sprite('LanternSleeve','tiny-factory',115,-14,-4,.5,color=AMBER)
    s.sprite('WorkshopNote','tiny-town',83,-17,-3,.4,color=WHITE)
    s.text('Workshop sign','MARA\'S WORKSHOP',-18,-6.4,.22,WHITE,z=.5)
    shutter(s,0,0,-3)
    # Drain latch releases the upper inspection ledge; no walk around its wall.
    for x in range(-19,0):
        if x==-8:continue
        s.sprite('Stone'+str(stone),'tiny-dungeon',2,x,2,.4,color=COLD,solid=True);stone+=1
    s.sprite('DrainGate','tiny-dungeon',47,-8,2,.4,color=COLD,solid=True)
    pad(s,'FocusPad0',-8,0);pad(s,'FocusPad1',-3,3)
    s.sprite('DrainReceiver','tiny-factory',98,8,0,.4,color=AMBER)
    s.sprite('BridgeReceiver','tiny-factory',98,12,6,.4,color=AMBER)
    for x,y in [(8,3),(8,6)]:s.sprite('Fixed mirror '+str((x,y)),'tiny-dungeon',61,x,y,.5,color=WHITE)
    s.text('Drain label','DRAIN',7,-.7,.18,WHITE,z=.5)
    s.text('Bridge label','BRIDGE',11,7,.18,WHITE,z=.5)
    # Stairs behind a powered bridge gate; backed by authored retaining walls.
    for x in range(13,19):
        for y in (4,8):s.sprite('Stair enclosure '+str((x,y)),'tiny-dungeon',2,x,y,.4,color=COLD,solid=True)
    for y in (5,7):s.sprite('Stair west jamb '+str(y),'tiny-dungeon',13,13,y,.4,color=COLD,solid=True)
    s.sprite('CaveEntranceGate','tiny-dungeon',47,13,6,.4,color=COLD,solid=True)
    s.sprite('CaveExit','tiny-dungeon',39,16,6,.2,color=WHITE)
    for x,y in [(-17,6),(-15,7),(-13,7)]:pine(s,'Cliff pine '+str((x,y)),x,y,COLD)
    for x,y in [(13,-5),(14,-5),(-17,4)]:s.sprite('Cliff debris '+str((x,y)),'tiny-town',105,x,y,.1,color=COLD)
    s.sprite('RestMarker','tiny-town',83,-18,-4,.3,color=WHITE)
    s.entity('Checkpoint0',-17,-4,.55)
    hud(s,False);return s

def sea_caves():
    s=Scene('SeaCaves');background=s.image('Deep bedrock','bedrock-tile.png',0,0,-3,64,64,[.27,.3,.38,1]);background['SpriteRendererComponent']['Source']['TilingFactor']=64
    walk=lambda x,y: (-19<=x<=-7 and -7<=y<=8) or (-6<=x<=5 and -5<=y<=8) or (6<=x<=18 and -7<=y<=5) or (-10<=x<=8 and -7<=y<=-6)
    floor=s.entity('Cave floor — excavated galleries',z=-1)['Entity'];stone=0
    for y in range(-12,13):
        for x in range(-23,24):
            on=walk(x,y);adjacent=not on and any(walk(x+a,y+b) for a,b in [(1,0),(-1,0),(0,1),(0,-1)])
            if adjacent:
                north=walk(x,y-1);south=walk(x,y+1);east=walk(x-1,y);west=walk(x+1,y)
                tile=4 if north and west else 5 if north and east else 16 if south and west else 17 if south and east else 2 if north else 26 if south else 13 if west else 15
                s.sprite('Stone'+str(stone),'tiny-dungeon',tile,x,y,.2,color=CAVE,solid=True);stone+=1
            else:s.sprite(('Gallery floor ' if on else 'Bedrock ')+str((x,y)),'tiny-town' if on and ((-4<=x<=3 and 2<=y<=7) or (x>=13 and -5<=y<=-2)) else 'tiny-dungeon',109 if on and ((-4<=x<=3 and 2<=y<=7) or (x>=13 and -5<=y<=-2)) else 24 if on and (x,y) in {(-18,7),(-17,7),(-18,6),(-8,7),(-7,7),(6,4),(7,4),(17,4),(18,4),(18,3)} else 0,x,y,0,color=CAVE if on else [.27,.3,.38,1],parent=floor)
    for x in range(-16,-11):
        for y in range(0,3):s.sprite('Cave seep '+str((x,y)),'tiny-battle',37,x,y,-.5,color=CAVE,solid=True)
    for x in range(-17,-10):
        for y in (-1,3):
            tile=(96 if x==-17 else 98 if x==-11 else 97) if y==3 else (120 if x==-17 else 122 if x==-11 else 121)
            s.sprite('Seep stone edge '+str((x,y)),'tiny-town',tile,x,y,-.45,color=CAVE,solid=True)
    for y in range(0,3):
        for x in (-17,-11):s.sprite('Seep stone edge '+str((x,y)),'tiny-town',108 if x==-17 else 110,x,y,-.45,color=CAVE,solid=True)
    # Old retaining columns give the relay gallery cover and a readable movement route.
    for x,y in [(-3,0),(2,5),(3,-2)]:s.sprite('Stone'+str(stone),'tiny-dungeon',6,x,y,.3,color=CAVE,solid=True);stone+=1
    shutter(s,1,-6,0);shutter(s,2,10,0)
    pad(s,'FocusPad2',-12,6);pad(s,'FocusPad3',0,3)
    s.sprite('GalleryReceiver','tiny-factory',98,-1,6,.4,color=AMBER)
    # Gate seals the east passage; north/south gallery shoulders are solid cliff.
    for y in range(-5,6):
        if y==0:continue
        if y==3:s.sprite('Gallery inspection glass','tiny-town',125,5,y,.4,color=WHITE,solid=True)
        else:s.sprite('Stone'+str(stone),'tiny-dungeon',13,5,y,.3,color=CAVE,solid=True);stone+=1
    for x in range(-5,5):s.sprite('Stone'+str(stone),'tiny-dungeon',26,x,-5,.3,color=CAVE,solid=True);stone+=1
    s.sprite('GalleryGate','tiny-dungeon',47,5,0,.4,color=CAVE,solid=True)
    s.sprite('ReturnGate','tiny-dungeon',47,-6,-6.5,.4,1,2,CAVE,solid=True)
    for x,y in [(8,3),(8,-3)]:
        e=s.sprite('Fixed mirror '+str((x,y)),'tiny-dungeon',61,x,y,.5,color=WHITE);e['TransformComponent']['Rotation'][2]=1.5707963267948966
    # Mara's alcove has a transparent optical window and a separate locked door.
    for y in range(-7,6):
        if y==-4:continue
        if y==-3:s.sprite('Inspection glass','tiny-town',125,12,y,.4,color=WHITE,solid=True)
        else:s.sprite('Stone'+str(stone),'tiny-dungeon',13,12,y,.3,color=CAVE,solid=True);stone+=1
    s.sprite('MaraDoor','tiny-dungeon',47,12,-4,.4,color=CAVE,solid=True)
    s.sprite('RescueReceiver','tiny-factory',98,13,-3,.4,color=AMBER)
    s.sprite('Mara','tiny-dungeon',98,14,-4,.6,color=WHITE)
    s.sprite('SurveyLedger','tiny-dungeon',63,15,-5,.3,color=WHITE)
    for x,y,t in [(16,-5,73),(17,-5,75),(15,-2,113),(-17,3,63),(-18,3,73),(8,-5,75)]:s.sprite('Cave salvage '+str((x,y)),'tiny-dungeon',t,x,y,.2,color=CAVE)
    s.sprite('StormWisp','tiny-dungeon',121,0,0,.7,color=WHITE)
    s.sprite('RushMark','tiny-battle',61,0,0,.1,1.6,1.6,WHITE)
    s.sprite('DryExit','tiny-dungeon',39,-17,-6,.2,color=WHITE)
    s.sprite('RestMarker','tiny-town',83,-14,-5,.3,color=WHITE)
    s.text('Gallery label','RELAY GALLERY',-3,7,.22,WHITE,z=.5)
    s.text('Passage label','DRY PASSAGE',-3,-6.8,.2,WHITE,z=.5)
    s.text('Mara ledge label','INSPECTION LEDGE',13,4,.18,WHITE,z=.5)
    s.entity('Checkpoint1',-12,-5,.55);s.entity('Checkpoint2',7,-4,.55)
    hud(s,True);return s

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--overwrite',action='store_true');args=p.parse_args()
    for scene in [cistern(),sea_caves()]:scene.save(args.overwrite);print(scene.name,len(scene.entities),'authored entities')
    from compose import menu
    endpoint=menu('CliffDawn',True)
    endpoint.sprite('Mara at landing','tiny-dungeon',98,11,-2,-.1,color=WHITE)
    endpoint.save(args.overwrite)
