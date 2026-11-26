#!/usr/bin/env python3
"""Rebuild the isolated CSM study; leaves all accepted lighting/AO/TAA fixtures frozen."""
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parents[2]
project=ROOT/'examples/RenderingReference'
source=(project/'Assets/Scenes/Direct_v1-PerspectivePBR.hazel').read_text()
blocks=re.split(r'(?=  - Entity:)',source)
camera=blocks[1].replace('Frozen PerspectivePBR camera','CSM perspective camera').replace('Translation: [0, 0, 10]','Translation: [10, 8, 14]').replace('Rotation: [0, 0, 0]','Rotation: [-0.435, 0.620, 0]')
light=blocks[2].replace('Direct only key / no environment','CSM directional key').replace('[-0.449999988, 0.600000024, 0]','[-0.9, 0.5, 0]')
entities=[camera,light]
def mesh(identity,name,position,scale,asset,materials='[]',rotation='[0, 0, 0]'):
    return f'''  - Entity: {identity}
    Relationship:
      Parent: 0
      Order: {identity}
    TagComponent:
      Tag: {name}
    TransformComponent:
      Translation: {position}
      Rotation: {rotation}
      Scale: {scale}
    MeshRendererComponent:
      Version: 1
      Mesh: {asset}
      Materials: {materials}
'''
cube='Native/Direct_v1/UnitCube.hmesh'
sphere='Reference/Direct_v1/Sphere.hmesh'
entities.append(mesh(10,'Contact receiver / large ground',[0,-.1,-25],[100,.2,100],cube))
entities.append(mesh(11,'Hard cube contact',[-3,1,0],[2,2,2],cube))
entities.append(mesh(12,'Authored smooth sphere contact',[0,1,0],[1,1,1],sphere))
entities.append(mesh(13,'Mirrored nonuniform cube',[3,1,0],[-2,2,1],cube,rotation='[0, 0.4, 0]'))
entities.append(mesh(14,'Thin tall caster / light leak study',[-4,3,-7],[.1,6,4],cube))
entities.append(mesh(15,'Masked caster',[4,2,-5],[3,4,.1],cube,'\n        - Slot: 0000000000000007\n          Material: Reference/Direct_v1/Surfaces/Mask.hmat'))
for i,z in enumerate((-15,-30,-50,-70)):
    entities.append(mesh(20+i,'Cascade distance '+str(-z),[0,2,z],[4,4,4],cube))
entities=[re.sub(r'(?m)^      Order: \d+$',f'      Order: {i}',block) for i,block in enumerate(entities)]
text='''SceneVersion: 3
Scene: Directional CSM / contacts and cascade distance
Presentation:
  Mode: SceneReferred
  Exposure: 0
  Background: [0.008, 0.008, 0.012, 1]
  Environment:
    Asset: Reference/IBL_v1/Constant.henv
    Yaw: 0
    Gain: 0.2
    Lighting: true
    Background: false
  Shadows:
    Enabled: true
    Resolution: 1024
    Distance: 100
    SplitLambda: 0.65
    NormalBias: 0.2
    DepthBias: 0.05
    Blend: 0.1
Entities:
'''+''.join(entities)
(project/'Assets/Scenes/DirectionalShadows_v1.hazel').write_text('\n'.join(line.rstrip() for line in text.splitlines())+'\n')
config=(project/'RenderingReference.hproj').read_text().replace('Name: Rendering Reference','Name: Directional shadow study').replace('Scenes/Core_v1-Orthographic.hazel','Scenes/DirectionalShadows_v1.hazel')
(project/'RenderingReference-Shadows.hproj').write_text(config+'\n' if not config.endswith('\n') else config)
print('Generated isolated directional shadow study scene and project')
