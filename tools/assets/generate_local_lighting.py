#!/usr/bin/env python3
"""Rebuild the separately authored LocalLights_v1 showcase; never alters frozen fixtures."""
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parents[2]
project=ROOT/'examples/RenderingReference'
source=(project/'Assets/Scenes/Direct_v1-PerspectivePBR.hazel').read_text()
blocks=re.split(r'(?=  - Entity:)',source)
entities=[]
camera=blocks[1].replace('Frozen PerspectivePBR camera','Local lights / material gallery').replace('Translation: [0, 0, 10]','Translation: [0, 0, 12]')
entities.append(camera)
for block in blocks[3:15]:
    if 'MeshRendererComponent:' in block:entities.append(block)
def entity(identity,name,position,rotation,scale,component):
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
{component}'''
for i,(p,c,intensity) in enumerate([([-3,1,2],[1,.12,.04],18),([3,-.6,2],[.05,.25,1],22),([0,3,1.5],[.2,1,.45],10)]):
    body=f'''    PointLightComponent:
      Color: {c}
      Intensity: {intensity}
      Enabled: true
      Range: 6
'''
    if i==0:body+='    ScriptComponent:\n      ClassName: RenderingReference.LocalLightMotion\n'
    entities.append(entity(200+i,['Moving warm key','Cool overlapping fill','Emerald rim'][i],p,[0,0,0],[1,1,1],body))
for i,(x,inner,outer) in enumerate([(-2,.18,.34),(2,.5,.85)]):
    entities.append(entity(210+i,'Narrow spot' if i==0 else 'Broad spot',[x,2.5,4],[-.45,0,0],[1,1,1],f'''    SpotLightComponent:
      Color: {[1,.7,.3] if i==0 else [.4,.65,1]}
      Intensity: 45
      Enabled: true
      Range: 9
      InnerCone: {inner}
      OuterCone: {outer}
'''))
entities.append(entity(220,'Neutral gallery backdrop',[0,0,-1.1],[0,0,0],[12,7,.2],'''    MeshRendererComponent:
      Version: 1
      Mesh: Native/Direct_v1/UnitCube.hmesh
      Materials: []
'''))
entities=[re.sub(r'(?m)^      Order: \d+$',f'      Order: {i}',block) for i,block in enumerate(entities)]
text='''SceneVersion: 3
Scene: LocalLights_v1 / chromatic material gallery
Presentation:
  Mode: SceneReferred
  Exposure: 0
  Background: [0.008, 0.008, 0.012, 1]
Entities:
'''+''.join(entities)
(project/'Assets/Scenes/LocalLights_v1.hazel').write_text(text)
config=(project/'RenderingReference.hproj').read_text().replace('Name: Rendering Reference','Name: Local lighting gallery').replace('Scenes/Core_v1-Orthographic.hazel','Scenes/LocalLights_v1.hazel')
(project/'RenderingReference-LocalLights.hproj').write_text(config)
print('Generated LocalLights_v1 scene and project (frozen sources unchanged)')
