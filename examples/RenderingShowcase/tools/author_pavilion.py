#!/usr/bin/env python3
"""Produce the original Pavilion architecture and compose the native scenes."""
import argparse
from collections import defaultdict
from copy import deepcopy
import json
from math import atan2, cos, hypot, pi, radians, sin, tau
from pathlib import Path
import random
import shutil
import struct
import subprocess

import yaml
from PIL import Image, ImageDraw, ImageFilter, ImageFont

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT.parents[1]
ASSETS = PROJECT / 'Assets'
PAVILION = ASSETS / 'Pavilion'


class Flow(list):
    """Compact YAML vectors."""
yaml.SafeDumper.add_representer(Flow, lambda d, v: d.represent_sequence(
    'tag:yaml.org,2002:seq', v, flow_style=True))


def write(path, document):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(yaml.safe_dump(document, sort_keys=False, width=110), encoding='utf-8')


def vector(values):
    return Flow(round(x, 7) for x in values)


def orient(eye, at):
    x, y, z = (a-b for a, b in zip(eye, at))
    return (atan2(-y, hypot(x, z)), atan2(x, z), 0)


def add(a, b, s=1):
    return tuple(x+s*y for x, y in zip(a, b))


class Architecture:
    def __init__(self):
        self.parts = defaultdict(lambda: ([], [], [], []))

    def quad(self, part, center, u, v, normal, repeat=(1, 1)):
        pos, norms, uvs, indices = self.parts[part]
        i = len(pos)
        for a, b in ((-.5,-.5), (.5,-.5), (.5,.5), (-.5,.5)):
            pos.append(add(add(center, u, a), v, b))
            norms.append(normal)
            uvs.append(((a+.5)*repeat[0], (.5-b)*repeat[1]))
        indices.extend(i+j for j in (0, 1, 2, 0, 2, 3))

    def box(self, part, center, size, tile=2):
        x, y, z = size
        for offset, u, v, n, lengths in (
            ((x/2,0,0),(0,0,-z),(0,y,0),(1,0,0),(z,y)),
            ((-x/2,0,0),(0,0,z),(0,y,0),(-1,0,0),(z,y)),
            ((0,y/2,0),(x,0,0),(0,0,-z),(0,1,0),(x,z)),
            ((0,-y/2,0),(x,0,0),(0,0,z),(0,-1,0),(x,z)),
            ((0,0,z/2),(x,0,0),(0,y,0),(0,0,1),(x,y)),
            ((0,0,-z/2),(-x,0,0),(0,y,0),(0,0,-1),(x,y)),
        ):
            self.quad(part, add(center,offset), u, v, n, tuple(s/tile for s in lengths))

    def ring(self, part, radius, tube, plane, tilt=0, sweep=tau):
        # Parametric annular bronze sections with smooth tangents and real tube profiles.
        pos, norms, uvs, indices = self.parts[part]
        count, sides = 96, 12
        start = len(pos)
        for i in range(count+1):
            a = i*sweep/count
            for j in range(sides+1):
                b = j*tau/sides
                radial = radius + tube*cos(b)
                p = (radial*cos(a), radial*sin(a), tube*sin(b))
                n = (cos(a)*cos(b), sin(a)*cos(b), sin(b))
                def rotate(v):
                    x,y,z = v if plane == 'vertical' else (v[0],v[2],-v[1])
                    return (x, y*cos(tilt)-z*sin(tilt), y*sin(tilt)+z*cos(tilt))
                pos.append(rotate(p))
                norms.append(rotate(n))
                uvs.append((i/count,j/sides))
                k = start+i*(sides+1)+j
                if i and j:
                    indices.extend((k-sides-2,k-1,k,k-sides-2,k,k-sides-1))

    def glb(self, path):
        doc = dict(asset=dict(version='2.0', copyright='Original Hazel Pavilion architecture, Apache-2.0'),
                   scene=0, scenes=[dict(nodes=[])], accessors=[], bufferViews=[], nodes=[], meshes=[])
        buffer = bytearray()

        def accessor(rows, shape, geom=False):
            width = len(rows[0])
            component, usage, fmt = (5125, 34963, 'I') if shape == 'SCALAR' else (5126, 34962, 'f')
            payload = b''.join(struct.pack('<'+fmt*width, *row) for row in rows)
            view = len(doc['bufferViews'])
            doc['bufferViews'].append(dict(buffer=0, byteOffset=len(buffer), byteLength=len(payload), target=usage))
            buffer.extend(payload)
            data = dict(bufferView=view, componentType=component, count=len(rows), type=shape)
            if geom:
                data.update(min=list(map(min,zip(*rows))), max=list(map(max,zip(*rows))))
            doc['accessors'].append(data)
            return len(doc['accessors'])-1

        for (group, surface), (p,n,uv,i) in self.parts.items():
            name = group + ' / ' + surface
            primitive = dict(attributes=dict(POSITION=accessor(p,'VEC3',True),
                                             NORMAL=accessor(n,'VEC3'), TEXCOORD_0=accessor(uv,'VEC2')),
                             indices=accessor([(index,) for index in i],'SCALAR'))
            doc['scenes'][0]['nodes'].append(len(doc['nodes']))
            doc['nodes'].append(dict(name=name, mesh=len(doc['meshes'])))
            doc['meshes'].append(dict(name=name, primitives=[primitive]))
        doc['buffers'] = [dict(byteLength=len(buffer))]
        metadata = json.dumps(doc, separators=(',', ':')).encode()
        metadata += b' '*(-len(metadata)%4)
        path.write_bytes(struct.pack('<4sII', b'glTF', 2, 28+len(metadata)+len(buffer)) +
                         struct.pack('<I4s', len(metadata), b'JSON') + metadata +
                         struct.pack('<I4s', len(buffer), b'BIN\0') + buffer)


def architecture():
    a = Architecture()
    def box(group, surface, center, size, tile=2):
        a.box((group,surface), center, size, tile)

    box('Courtyard','Aggregate',(0,-.13,12),(24,.24,16))
    for z in (7.5,9,10.5,12,13.5,15,16.5,18):
        box('Courtyard paving joints','Black',(0,-.008,z),(20,.012,.018))
    for x in (-9,-4.1,4.1,9):
        box('Courtyard bronze inlay','Bronze',(x,.002,12),(.025,.01,15))
    for x in (-9.7,9.7):
        box('Boundary','Concrete',(x,.43,11.3),(.5,.86,16))
        for z in (9,14.6):
            box('Planter concrete','Concrete',(x*.85,.25,z),(1.45,.5,3.3))
            box('Planter infill','Black',(x*.85,.505,z),(1.33,.02,3.15))
        for z in (7.7,12.6,17.5):
            box('Promenade bollards','Black',(x*.64,.24,z),(.15,.48,.15))
            box('Promenade luminaires','Amber',(x*.64,.50,z),(.12,.032,.12))
    for x in (-5.7,6.2):
        box('Benches','Oak',(x,.47,10.8),(2.2,.11,.52),1.6)
        for dx in (-.83,.83):
            box('Benches','Black',(x+dx,.21,10.8),(.14,.42,.43))

    for x in (-6.9,6.9):
        box('Entrance portal','Concrete',(x,2.45,5.7),(1.1,4.9,3.8))
        box('Entrance portal','Bronze',(x*.9,2.42,6.78),(.09,4.84,.12))
    box('Overhanging canopy','Concrete',(0,5.15,4.3),(16.5,.44,7.2))
    box('Canopy fascia','Black',(0,4.855,7.86),(16.5,.18,.11))
    box('Canopy linear light','Amber',(0,4.71,7.72),(15.8,.038,.05))
    for i in range(45):
        x = (i-22)*.36
        box('Canopy soffit','Oak',(x,4.78,5.45),(.2,.16,4.52),2.4)
    for x in (-5.25,5.25):
        box('Entry return wall','Concrete',(x,2.3,4.7),(2.2,4.6,.4))
    box('Threshold','Bronze',(0,.015,5.7),(8.2,.034,.095))
    box('Gallery terrazzo','Marble',(0,-.10,-.52),(14,.2,13.24),1.5)
    for x in (-5.9,5.9):
        box('Floor guide rails','Bronze',(x,.007,-.7),(.035,.014,13.5))
    box('Gallery roof','Black',(0,5.11,-2.5),(14.9,.34,8.7))
    box('Gallery side wall','Concrete',(7.0,2.5,-.9),(.38,5,13.7))
    for z in (-6.3,-3.8,-1.3,1.2,3.7,5.9):
        box('Window frames','Bronze',(-6.82,2.56,z),(.28,4.6,.091))
    for y in (.34,4.63):
        box('Window sills','Concrete',(-6.93,y,-.3),(.52,.34,13.5))
    for z in (-5.0,-2.5,0,2.5,5):
        box('Gallery fins','Oak',(-6.4,2.46,z),(.34,4.45,.13),1.8)
    for z in (-5.6,-2.9,-.2,2.5):
        box('Ribbed gallery ceiling','Black',(0,4.84,z),(13.8,.21,.30))
        box('Gallery ceiling coves','Warm',(0,4.708,z),(11.5,.031,.056))
    for x in (-6.73,6.73):
        box('Recessed skirting','Amber',(x,.20,-1.1),(.034,.035,11.3))
    box('Exhibition back wall','Concrete',(-1.81,2.52,-7.2),(10.24,5.02,.45))
    box('Exhibition back wall','Concrete',(6.1,2.52,-7.2),(1.82,5.02,.45))
    box('Passage lintel','Concrete',(4.28,4.20,-7.2),(1.9,1.66,.45))
    for x in (3.34,5.23):
        box('Passage frame','Bronze',(x,1.69,-6.92),(.09,3.38,.18))
        box('Passage frame','Amber',(x,1.69,-6.815),(.02,3.17,.023))
    box('Exhibition panel','Slate',(-2.2,2.86,-6.91),(5.85,2.82,.14))
    box('Exhibition panel','Bronze',(-2.2,1.42,-6.83),(5.85,.035,.06))
    for x in (-5.9,1.56):
        for z in (-6.86,-6.63):
            box('Exhibition wood screens','Oak',(x,2.42,z),(.19,4.27,.12),1.5)

    box('Exhibit pedestal','Black',(-1.25,.47,-2.55),(2.87,.94,1.72))
    box('Exhibit pedestal','Bronze',(-1.25,.975,-2.55),(2.93,.07,1.78))
    box('Exhibit pedestal','Warm',(-1.25,.16,-1.684),(2.62,.031,.03))
    box('Exhibit pedestal','Slate',(-1.25,1.084,-2.55),(.93,.15,.83))
    box('Foreground rail','Bronze',(-2.03,1.18,-.65),(1.45,.07,.13))
    for x in (-2.69,-1.38):
        box('Foreground rail','Black',(x,.57,-.65),(.056,1.14,.065))
    for x in (1.60,2.25):
        box('Secondary display','Slate',(x,.41,-3.5),(.55,.82,.57))
        box('Secondary display','Bronze',(x,.85,-3.5),(.58,.06,.6))
    a.ring(('Orbital sculpture outer','Bronze'),1.60,.048,'vertical',.28)
    a.ring(('Orbital sculpture inner','Steel'),1.41,.032,'vertical',-.37)
    a.ring(('Orbital sculpture arc','Warm'),1.48,.016,'vertical',.27,4.05)
    a.ring(('Kinetic exhibit','Bronze'),.24,.025,'vertical',.15)
    a.ring(('Archive lens housing','Steel'),.165,.055,'vertical',-.11)
    a.ring(('Archive lens rim','Bronze'),.141,.018,'vertical',-.11)
    # Short black radial flutes silhouette the barrel in the kinetic composition.
    for x in (2.12,2.18,2.24,2.30,2.36):
        box('Archive lens mount','Black',(x,.935,-3.50),(.038,.17,.37))


    xc = 4.28
    box('Passage','Slate',(xc,-.095,-11.245),(1.9,.19,8.21),1.5)
    box('Passage','Black',(xc,3.45,-11.2),(2.43,.19,8.3))
    for x in (3.19,5.39):
        box('Passage','Concrete',(x,1.68,-11.22),(.33,3.36,8.31))
        for i in range(23):
            z = -7.3 - i*.35
            box('Passage ribbing','Oak',(x+(.178 if x<xc else -.178),1.68,z),(.075,3.1,.125))
        box('Passage wayfinding','Cold',(x+(.21 if x<xc else -.21),.24,-11.6),(.032,.042,7.03))
    box('Passage terminus','Slate',(xc,1.65,-15.25),(2.18,3.3,.24))
    for z in (-8.17,-10.25,-12.35,-14.42):
        box('Passage ceiling practicals','Warm',(xc,3.32,z),(1.32,.049,.067))
    box('Passage plate','Black',(xc,2.01,-15.10),(1.69,.65,.10))
    for group, surface, center, width, height in (
        ('Passage lightbox','Exit',(xc,2.01,-15.022),1.55,.51),
        ('Entrance sign','Title',(0,4.93,7.915),4.7,.43),
        ('Exhibit sign','Exhibition',(-2.23,2.96,-6.817),5.35,1.58),
        ('Artifact label','Label',(-1.25,.63,-1.685),1.52,.34),
    ):
        a.quad((group,surface),center,(width,0,0),(0,height,0),(0,0,1))

    rng = random.Random(11)
    for x in (-8.24,8.24):
        for z in (9,14.6):
            for j in range(17):
                y = .62 + rng.random()*.63
                width = .27 + rng.random()*.30
                angle = rng.random()*tau
                normal = (sin(angle),0,cos(angle))
                u = (width*cos(angle),0,-width*sin(angle))
                a.quad(('Court grasses','Leaves'),(x+rng.uniform(-.4,.4),y,z+rng.uniform(-1.3,1.3)),
                       u, (0,.59+rng.random()*.46,0),normal)
    # Hazel retains at most 32 distinct mesh entries per project view. Merge
    # the static opaque architecture by material, preserving moving geometry.
    result = Architecture()
    for (group, surface), (pos, norms, uvs, indices) in a.parts.items():
        keep = group.startswith(('Orbital ', 'Kinetic ', 'Court grasses', 'Archive lens housing', 'Archive lens rim'))
        dest = (group if keep else 'Architecture', surface)
        p, n, uv, idx = result.parts[dest]
        idx.extend(i+len(p) for i in indices)
        p.extend(pos)
        n.extend(norms)
        uv.extend(uvs)
    return result


def material(name, color, rough=.5, metal=0, emissive=(0,0,0), tex=None,
             normal=.5, alpha='Opaque', double=False):
    textures = {}
    for slot in ('BaseColor','MetallicRoughness','Normal','Occlusion','Emissive'):
        image = (tex or {}).get(slot, '')
        path = image if image.startswith('Materials/') else 'Pavilion/Surfaces/'+image if image else ''
        textures[slot] = dict(Image=path, Min='LinearMipmapLinear',
                              Mag='Linear', S='Repeat', T='Repeat')
    strength = max(1, *emissive)
    write(PAVILION/'Surfaces'/f'{name}.hmat', dict(Material=dict(Version=1, Name=name,
          Model='MetallicRoughness', BaseColor=vector((*color,1)), Emissive=vector(v/strength for v in emissive), EmissiveStrength=strength,
          Metallic=metal, Roughness=rough, NormalScale=normal, OcclusionStrength=.8,
          Alpha=alpha, AlphaCutoff=.4, DoubleSided=double, Textures=textures)))


def surfaces(source):
    folder = PAVILION/'Surfaces'
    folder.mkdir(parents=True, exist_ok=True)
    for name, asset, color, roughness, normal in (
        ('Marble','marble_01',(.48,.53,.56),.27,.12),
        ('Aggregate','concrete_floor_02',(.68,.62,.52),.88,.65),
        ('Concrete','concrete_wall_007',(.62,.54,.43),.87,.26),
    ):
        maps = {}
        for slot, map_name in [('BaseColor','color'),('MetallicRoughness','arm'),('Normal','normal'),('Occlusion','arm')]:
            target = f'{asset}-{map_name}.jpg'
            shutil.copyfile(source/asset/(map_name+'.jpg'), folder/target)
            maps[slot] = target
        material(name,color,roughness,1,tex=maps,normal=normal)
    material('Black',(.025,.031,.034),.36,.22)
    material('Slate',(.038,.054,.061),.32,.10)
    material('Bronze',(.62,.32,.14),.22,.88)
    material('Steel',(.53,.59,.65),.25,1)
    material('Warm',(.28,.14,.042),.5,emissive=(7.8,4.4,1.66))
    material('Amber',(.32,.15,.036),.5,emissive=(3.9,1.66,.48))
    material('Cold',(.05,.20,.26),.5,emissive=(.16,1.45,2.65))
    oak = dict(BaseColor='Materials/wood_table_001/Diffuse.jpg',
               MetallicRoughness='Materials/wood_table_001/arm.jpg',
               Normal='Materials/wood_table_001/nor_gl.jpg')
    material('Oak',(.54,.35,.20),.8,1,tex=oak,normal=.38)

    fonts = ROOT/'Hazel/Resources/fonts/opensans'
    def sign(name, size, rows, color, background, gain):
        image = Image.new('RGB',size,background)
        draw = ImageDraw.Draw(image)
        for words, x, y, pixels, weight in rows:
            font = ImageFont.truetype(str(fonts/f'OpenSans-{weight}.ttf'), pixels)
            draw.text((x,y),words,font=font,fill=color,anchor='lt')
        image.save(folder/(name+'.png'), optimize=True)
        material(name,(.42,.42,.42),.60,emissive=(gain,gain,gain),
                 tex={'BaseColor':name+'.png', 'Emissive':name+'.png'})

    sign('Title',(1024,96),[('T H E   P A V I L I O N',131,15,65,'Light')],
         (246,207,151),(36,30,24),1.5)
    sign('Exhibition',(1024,304),[
        ('PAVILION  /  PERMANENT COLLECTION',35,30,28,'SemiBold'),
        ('NIGHT FLIGHT',29,87,105,'Light'),
        ('THE ART OF SEEING IN THE DARK',36,226,37,'Regular')],
        (247,217,171),(42,49,52),1.02)
    sign('Label',(768,176),[
        ('A–11  /  FLIGHT HELMET',29,22,49,'SemiBold'),
        ('LEATHER, BRONZE & OPTICAL GLASS',31,105,29,'Light')],
        (245,207,157),(48,32,22),.85)
    sign('Exit',(768,256),[
        ('02    /    OPTICS',31,35,72,'Light'), ('NIGHT FLIGHT  /  ARCHIVE',36,153,36,'Regular')],
        (162,230,240),(27,50,60),3.7)

    leaves = Image.new('RGBA',(128,256))
    draw = ImageDraw.Draw(leaves)
    rng = random.Random(49)
    for _ in range(32):
        x = rng.randint(16,109)
        y = rng.randint(4,148)
        base = rng.randint(44,83)
        shade = rng.randint(45,104)
        draw.polygon(((base,255),(x,y),(x+2,y+14),(base+3,255)),
                     fill=(shade,shade+12,shade//2,255))
    leaves.save(folder/'Grasses.png', optimize=True)
    material('Leaves',(.74,.78,.58),.91,tex={'BaseColor':'Grasses.png'},alpha='Mask',double=True)

    rng = random.Random(240)
    dirt = Image.new('L',(256,256),0)
    draw = ImageDraw.Draw(dirt)
    for _ in range(140):
        x,y,r = rng.randrange(256),rng.randrange(256),rng.uniform(.3,3.4)
        draw.ellipse((x-r,y-r,x+r,y+r),fill=rng.randrange(20,132))
    dirt.filter(ImageFilter.GaussianBlur(1)).convert('RGB').save(folder/'OpticalDust.png',optimize=True)
    # A denser optical plate is used only for the passage demonstration.
    for _ in range(42):
        x,y,r = rng.randrange(256),rng.randrange(256),rng.uniform(3,11)
        draw.ellipse((x-r,y-r,x+r,y+r),fill=rng.randrange(42,168))
    dirt.filter(ImageFilter.GaussianBlur(2.0)).convert('RGB').save(folder/'TourDust.png',optimize=True)


    with (folder/'NightFlight.cube').open('w') as lut:
        lut.write('TITLE "Night Flight"\nLUT_3D_SIZE 17\n')
        for b in range(17):
            for g in range(17):
                for r in range(17):
                    red,green,blue = r/16,g/16,b/16
                    luma = .2126*red+.7152*green+.0722*blue
                    edge = luma*(1-luma)
                    shifts = (red + edge*(.18*luma-.033), green + edge*.018,
                              blue + edge*(.12-.22*luma))
                    lut.write(' '.join(f'{min(1,max(0,v)):.7f}' for v in shifts)+'\n')


def compose():
    entities = []
    order = defaultdict(int)
    def entity(tag, pos=(0,0,0), rot=(0,0,0), scale=(1,1,1), parent=0, **components):
        eid = len(entities)+1
        entities.append(dict(Entity=eid, Relationship=dict(Parent=parent,Order=order[parent]),
                             TagComponent=dict(Tag=tag), TransformComponent=dict(
                                 Translation=vector(pos), Rotation=vector(rot), Scale=vector(scale)),
                             **components))
        order[parent] += 1
        return eid

    grade = dict(Version=1, WhiteKelvin=5900, TintUV=.0011, Saturation=.93,
                 Contrast=.13, Shadows=-.10, Highlights=.04,
                 LUT='Pavilion/Surfaces/NightFlight.cube', LUTStrength=.65)
    optics = dict(Version=1, FlareStrength=.065, GhostCount=3, GhostSpacing=.32,
                  HaloStrength=.076, HaloRadius=.41, Starburst=.11, AnamorphicStreak=.14,
                  DirtTexture='Pavilion/Surfaces/OpticalDust.png', DirtStrength=.09, DirtScale=1,
                  Vignette=.15)
    views = [
        ('01 Arrival', (9.4,2.22,16.5), (.0,2.61,4.7), 27),
        ('02 Threshold', (.4,1.65,8.7), (-1.3,1.7,-4.6), 25),
        ('03 Night Flight', (3.2,2.5,4.9), (-1.54,2.3,-3.15), 25),
        ('04 Reflection sweep', (2.45,.68,3.6), (-1.28,1.07,-2.6), 25),
        ('05 Kinetic exhibit', (3.10,1.54,-.32), (1.63,1.2,-3.57), 43),
        ('06 Archive passage', (4.03,1.55,-7.62), (4.27,1.75,-15.12), 28),
        ('07 Flight helmet', (-.70,1.72,-.76), (-1.22,1.57,-2.52), 63),
        ('08 Shutter sweep', (.92,1.54,.36), (-.48,1.54,-2.55), 39),
        ('09 Closing gallery', (-5.65,2.48,3.40), (-.85,2.26,-3.92), 25),
    ]
    for no,(tag,eye,at,focal) in enumerate(views,1):
        lens = dict(Version=1, Projection=True, FocalLengthMM=focal, SensorWidthMM=36,
                    SensorHeightMM=24, Aperture=1.3 if no==7 else 4, ShutterSeconds=.02,
                    ISO=500, FocusDistance=1.86 if no==7 else 6)
        settings = dict(Camera=dict(ProjectionType=0, PerspectiveFOV=.75, PerspectiveNear=.055,
                                    PerspectiveFar=120, OrthographicSize=10, OrthographicNear=-1,
                                    OrthographicFar=1, PhysicalLens=lens),
                        Primary=no==1, FixedAspectRatio=False, ColorGrade=deepcopy(grade))
        if no in (6,7,9):
            settings['LensEffects']=deepcopy(optics)
        if no==7:
            settings['DepthOfField']=dict(Version=1, Enabled=True, Quality='Cinematic',
                                          ApertureBlades=8, ApertureRotationRadians=.18,
                                          FocusHalfLifeSeconds=.23)
        if no in (5,8):
            settings['MotionBlur']=dict(Version=1, Enabled=True, Shutter='Angle',
                                        ShutterAngleDegrees=180, Quality='Cinematic')
        if no==6:
            settings['CameraExposure']=dict(Version=1, Mode='Automatic', CompensationStops=-.61,
                                            MinGainStops=-2, MaxGainStops=1.35, HighPercentile=.91)
        if no == 1:
            # Player tour inherits the camera's physical and post settings.
            settings['LensEffects'] = dict(optics, FlareStrength=.19, DirtStrength=1.35,
                                           DirtTexture='Pavilion/Surfaces/TourDust.png',
                                           AnamorphicStreak=.24, Starburst=.18)
            settings['DepthOfField'] = dict(Version=1, Enabled=True, Quality='Cinematic',
                                           ApertureBlades=8, ApertureRotationRadians=.18,
                                           FocusHalfLifeSeconds=.12)
            settings['MotionBlur'] = dict(Version=1, Enabled=True, Shutter='Angle',
                                           ShutterAngleDegrees=125, Quality='Cinematic')
        entity(tag,eye,orient(eye,at),CameraComponent=settings)

    entity('Late sunlight',rot=orient((0,0,0),(1,-.55,-.66)),
           DirectionalLightComponent=dict(Enabled=True,Color=vector((1,.68,.40)),Intensity=3.8))
    def spot(tag, eye, at, color, intensity, distance, cone, shadows=True):
        entity(tag,eye,orient(eye,at),SpotLightComponent=dict(
            Enabled=True,Color=vector(color),Intensity=intensity,Range=distance,
            InnerCone=cone*.62,OuterCone=cone,CastShadows=shadows,ShadowResolution=1024))
    def point(tag, eye, color, intensity, distance, shadows=False):
        entity(tag,eye,PointLightComponent=dict(
            Enabled=True,Color=vector(color),Intensity=intensity,Range=distance,
            CastShadows=shadows,ShadowResolution=512))
    spot('Exhibit warm key',(-.15,4.57,-.62),(-1.25,1.40,-2.55),(1,.68,.44),125,9,.40)
    spot('Exhibit cool edge',(-3.3,4.56,-3.65),(-1.23,1.55,-2.55),(.50,.74,1),95,8,.37)
    spot('Kinetic spotlight',(1.65,3.8,-3.18),(1.65,.92,-3.54),(1,.78,.55),67,6,.40)
    spot('Passage warm graze',(4.34,3.21,-10.30),(3.36,.42,-11.61),(1,.58,.30),72,6,.48)
    point('Gallery warm practical',(2.6,2.56,-4.8),(1,.60,.31),39,6,True)
    point('Passage blue practical',(4.57,2.49,-13.46),(.17,.54,1),11,3.45,True)
    for i,z in enumerate((-5.7,-2.9,-.20,2.5)):
        point(f'Ceiling bounce {i}',(1,4.05,z),(.80,.70,.59),55,6.7)
        point(f'Window bounce {i}',(-5.8,2.55,z),(.39,.56,.82),14,5.4)
    for z in (-8.38,-12.51,-14.43):
        point('Passage warm fill',(4.14,2.90,z),(1,.55,.26),6,3.5)
    for x in (-5.,5.):
        point('Entrance sconce',(x,3.7,5.25),(1,.57,.29),23,5.5)
    for x in (-6.16,6.16):
        for z in (7.7,12.6,17.5):
            point('Courtyard practical',(x,.62,z),(1,.51,.23),2.1,2.8)

    for tag,pos,reference,influence,offset,projection in (
        ('01 Gallery probe',(-1.9,2.05,-1.8),'Hall',(5.38,2.09,5.1),(1.90,.45,.89),(7.3,2.55,7.0)),
        ('02 Threshold probe',(4.28,1.60,-6.65),'Archive',(1.69,1.65,2.91),(0,.16,-.53),(1.85,1.87,3.55)),
    ):
        entity(tag,pos,ReflectionProbeComponent=dict(
            Environment=f'Pavilion/Reflections/{reference}.henv', Enabled=True,
            InfluenceHalfSize=vector(influence), ProjectionOffset=vector(offset),
            ProjectionHalfSize=vector(projection), BlendDistance=1.1))

    for part in yaml.safe_load((PAVILION/'Architecture/Model.hprefab').read_text())['Entities']:
        if 'MeshRendererComponent' not in part:
            continue
        group, surface = part['TagComponent']['Tag'].split(' / ')
        mesh = part['MeshRendererComponent']
        slot = yaml.safe_load((ASSETS/mesh['Mesh']).read_text())['Mesh']['Slots'][0]['ID']
        mesh['Materials'] = [dict(Slot=str(slot), Material=f'Pavilion/Surfaces/{surface}.hmat')]
        positions = {'Orbital sculpture outer':(-1.39,2.85,-3.52),
                     'Orbital sculpture inner':(-1.39,2.85,-3.52),
                     'Orbital sculpture arc':(-1.39,2.85,-3.52),
                     'Kinetic exhibit':(1.60,1.151,-3.5),
                     'Archive lens housing':(2.24,1.104,-3.48),
                     'Archive lens rim':(2.24,1.102,-3.42)}
        # The original title plane was exactly on the fascia's +Z surface
        # at 7.915 m. Mount it 35 mm forward on a shallow sign backing.
        if surface == "Title":
            positions[group] = (0,0,.035)
        entity(surface + " architecture" if group == "Architecture" else group,
               positions.get(group,(0,0,0)), MeshRendererComponent=mesh)
    entity('Title sign backing', (0,4.93,7.927), scale=(4.74,.46,.038),
           MeshRendererComponent=dict(Version=1, Mesh='Native/Direct_v1/UnitCube.hmesh',
               Materials=[dict(Slot='0000000000000007', Material='Pavilion/Surfaces/Black.hmat')]))

    hero = entity('A–11 flight helmet',(-1.25,1.159,-2.56),(0,.31,0))
    for part in yaml.safe_load((PAVILION/'FlightHelmet/Model.hprefab').read_text())['Entities']:
        if 'MeshRendererComponent' in part:
            entity(part['TagComponent']['Tag'],parent=hero,MeshRendererComponent=part['MeshRendererComponent'])
    entities[6]['CameraComponent']['DepthOfField']['TrackedSubject'] = entity('A–11 focus pivot',(-1.21,1.71,-2.46))
    entity('Archive focus mark',(-1.75,2.82,-6.81))
    entity('Foreground focus mark',(-1.45,1.19,-.65))

    presentation = dict(Mode='SceneReferred', CameraExposure=dict(
        Version=1,Mode='Automatic',CompensationStops=-.28,Metering='CenterWeighted',
        IncludeSky=False,LowPercentile=.1,HighPercentile=.94,MinGainStops=-2.4,MaxGainStops=3.3,
        BrighteningRate=1.35,DarkeningRate=2.20),
        TemporalAA=True, Background=vector((.009,.01,.014,1)),
        Environment=dict(Asset='Pavilion/Sunset.henv',Yaw=.94,Gain=.55,Lighting=True,Background=True),
        AmbientOcclusion=dict(Enabled=True,Quality=3,Radius=.65,Strength=.78,Falloff=.615),
        ScreenReflections=dict(Enabled=True,Quality=2,Steps=56,Distance=20,Thickness=.14),
        Bloom=dict(Enabled=True,Intensity=.095,Radius=.92),
        Shadows=dict(Enabled=True,Resolution=2048,Distance=56,SplitLambda=.65,
                     NormalBias=.20,DepthBias=.05,Blend=.15))
    scene = dict(SceneVersion=3,Scene='The Pavilion / Ultra',Presentation=presentation,Entities=entities)
    write(ASSETS/'Scenes/Pavilion.hazel',scene)

    high = deepcopy(scene)
    high['Scene']='The Pavilion / High'
    high['Presentation']['Shadows']['Resolution']=1024
    high['Presentation']['AmbientOcclusion']['Quality']=2
    high['Presentation']['ScreenReflections'].update(Quality=1,Steps=36)
    for e in high['Entities']:
        if 'CameraComponent' in e:
            camera=e['CameraComponent']
            for effect in ('DepthOfField','MotionBlur'):
                if effect in camera: camera[effect]['Quality']='Balanced'
        if 'SpotLightComponent' in e:
            e['SpotLightComponent']['ShadowResolution']=512
        if 'PointLightComponent' in e:
            e['PointLightComponent']['ShadowResolution']=256
    write(ASSETS/'Scenes/Pavilion-High.hazel',high)

    diagnostic=deepcopy(scene)
    diagnostic['Scene']='The Pavilion / Diagnostic'
    diagnostic['Presentation']['CameraExposure']=dict(Version=1, Mode='ManualPhysical', CompensationStops=0)
    diagnostic['Presentation']['TemporalAA']=False
    diagnostic['Presentation']['Bloom']['Enabled']=False
    for e in diagnostic['Entities']:
        if 'CameraComponent' in e:
            camera=e['CameraComponent']
            camera['Primary']=e['Entity']==4
            lens=camera['Camera']['PhysicalLens']
            lens.update(Aperture=4, ShutterSeconds=.015625, ISO=100)
            for key in ('CameraExposure','ColorGrade','LensEffects','DepthOfField','MotionBlur'):
                camera.pop(key,None)
    write(ASSETS/'Scenes/Pavilion-Diagnostic.hazel',diagnostic)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,help='Directory containing the downloaded Poly Haven maps')
    parser.add_argument('--native-tool',type=Path,help='HazelProject binary to cook original architecture into a new folder')
    args=parser.parse_args()
    if args.native_tool:
        if (PAVILION/'Architecture').exists():
            parser.error('Move the old Architecture directory aside before recooking.')
        import tempfile
        with tempfile.TemporaryDirectory() as temp:
            glb=Path(temp)/'Architecture.glb'
            architecture().glb(glb)
            subprocess.run([args.native_tool,'import-glb',PROJECT/'RenderingShowcase.hproj',
                            glb,'Pavilion/Architecture'],check=True)
    if args.source:
        surfaces(args.source)
    compose()


if __name__=='__main__':
    main()
