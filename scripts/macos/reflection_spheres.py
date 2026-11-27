#!/usr/bin/env python3
"""Isolate near-field mirror-sphere silhouettes with native captures and an FP64 oracle."""
import argparse
from collections import Counter
import csv
import json
import math
import os
from pathlib import Path
import shutil
import statistics
import struct
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import hazel as hz
from macos.screen_reflections import native_command, RESOURCES
from macos.static_probe_capture import read_cube
from macos.xegtao import pixels, png

WIDTH, HEIGHT, SIDE, POSES = 384, 256, 128, 9
ORANGE, RADIUS = (-1.68, 1.56, 1.62), .65
OFFSET, BOX = (.35, -.22, .31), (3., 2.8, 3.)
ZERO = (0., 0., 0.)
# Independent outward face frames: forward, increasing column, increasing row.
AXES = (
    ((1, 0, 0), (0, 0, -1), (0, -1, 0)),
    ((-1, 0, 0), (0, 0, 1), (0, -1, 0)),
    ((0, 1, 0), (1, 0, 0), (0, 0, 1)),
    ((0, -1, 0), (1, 0, 0), (0, 0, -1)),
    ((0, 0, 1), (1, 0, 0), (0, -1, 0)),
    ((0, 0, -1), (-1, 0, 0), (0, -1, 0)),
)
MODES = ("black", "ssr", "mixed", "box", "unprojected", "offset", "mixed-offset", "mixed-unprojected")


def add(a, b): return tuple(x + y for x, y in zip(a, b))
def sub(a, b): return tuple(x - y for x, y in zip(a, b))
def mul(a, f): return tuple(x * f for x in a)
def dot(a, b): return sum(x * y for x, y in zip(a, b))
def unit(a): return mul(a, 1 / math.sqrt(dot(a, a)))


def intersect(origin, ray, center, radius):
    # Projection onto the ray gives the closest point and the half chord.
    delta = sub(center, origin)
    along = dot(delta, ray)
    separation = dot(delta, delta) - along * along
    if separation >= radius * radius:
        return None
    half = math.sqrt(radius * radius - separation)
    for t in (along - half, along + half):
        if t > .000001:
            return t
    return None


def angle(a, b):
    return math.degrees(math.acos(max(-1, min(1, dot(a, b)))))


def sphere_margin(origin, ray):
    delta = sub(ORANGE, origin)
    return math.degrees(math.asin(RADIUS / math.sqrt(dot(delta, delta)))) - angle(unit(delta), ray)


def box_exit(point, ray):
    # Intersect finite planes, checking the two axes of each face independently.
    candidates = []
    for i, extent in enumerate(BOX):
        if ray[i] == 0:
            continue
        for plane in (-extent, extent):
            distance = (plane - point[i]) / ray[i]
            q = add(point, mul(ray, distance))
            if distance > 0 and all(abs(q[j]) <= BOX[j] + 1e-10 for j in range(3) if j != i):
                candidates.append((distance, q))
    return min(candidates)[1]


def cube_oracle(raw_path, origin, prefix):
    raw = read_cube(raw_path)
    good = mistakes = edge = corner = 0
    contour_error = []
    for f, (forward, right, down) in enumerate(AXES):
        colors = []
        face_hits = []
        for y in range(SIDE):
            for x in range(SIDE):
                local = add(mul(right, (2*x+1)/SIDE-1), mul(down, (2*y+1)/SIDE-1))
                d = unit(add(forward, local))
                margin = sphere_margin(origin, d)
                i = (f*SIDE*SIDE+y*SIDE+x)*3
                color = tuple(raw[i+c] for c in range(3))
                observed = color[0] > .25
                if abs(margin) > 1.0:
                    good += 1
                    mistakes += observed != (margin > 0)
                    if observed:
                        edge += min(x, y, SIDE-1-x, SIDE-1-y) < 2
                        corner += min(x, SIDE-1-x) < 2 and min(y, SIDE-1-y) < 2
                face_hits.append((observed, margin))
                colors.append((*color, 1, -1, 1))
        for y in range(1,SIDE-1):
            for x in range(1,SIDE-1):
                i=y*SIDE+x
                if face_hits[i][0] and any(not face_hits[i+d][0] for d in (-1,1,-SIDE,SIDE)):
                    contour_error.append(abs(face_hits[i][1]))
        # png expects bottom-up, whereas the cube artifact stores top-down rows.
        bottom_up = [pixel for y in reversed(range(SIDE)) for pixel in colors[y*SIDE:(y+1)*SIDE]]
        png(prefix.with_name(prefix.name+f"-{f}.png"), SIDE, SIDE, bottom_up)
    if mistakes or edge < 30 or not contour_error:
        raise RuntimeError(f"Raw cube disagrees with guarded FP64 directions: {raw_path}, {mistakes} errors")
    return dict(guarded=good, errors=mistakes, edgeHits=edge, cornerHits=corner,
                maxContourAngularErrorDegrees=max(contour_error))


def projection(vp, p):
    h = [sum(vp[col*4+row]*v for col,v in enumerate((*p,1))) for row in range(4)]
    return ((h[0]/h[3]+1)*WIDTH*.5, (h[1]/h[3]+1)*HEIGHT*.5, h[3])


def centroid(samples):
    if not samples:
        return None
    return tuple(statistics.mean(p[i] for p in samples) for i in range(2))


def contour(mask):
    return [(i%WIDTH,i//WIDTH) for i in mask if any(i+d not in mask for d in (-1,1,-WIDTH,WIDTH))]


def contour_error(a, b):
    if not a or not b:
        return None
    first, second = contour(a), contour(b)
    distances = sorted(min(math.hypot(x-u,y-v) for u,v in dst)
                       for src,dst in ((first,second),(second,first)) for x,y in src)
    return dict(p95=distances[int(.95*len(distances))], maximum=distances[-1])


def verify(folder, destination, diagnostic=None):
    results = []
    for row in csv.reader((folder/"cameras.csv").open()):
        pose = int(row[0])
        eye = tuple(map(float,row[1:4]))
        vp = list(map(float,row[4:]))
        directions = pixels(folder/f"normal-{pose}.bin")
        position = pixels(folder/f"position-{pose}.bin")
        baseline = pixels(folder/f"black-{pose}.bin")
        capture = {mode: pixels(folder/f"{mode}-{pose}.bin") for mode in MODES}
        trace = pixels(diagnostic/f"ssr-{pose}.bin") if diagnostic else None
        receiver = position[HEIGHT//2*WIDTH+WIDTH//2][4]
        if receiver < 0:
            raise RuntimeError("No chrome receiver at registration pixel")
        capture_masks = {mode: set() for mode in MODES}
        refs = {name: set() for name in ("ideal","box","offset","unprojected","screenVisible")}
        guarded = {name: [0,0,0] for name in ("box","offset","unprojected","ssr")}
        angles = {name: [] for name in ("box","offset","unprojected","meshReflectionDirection")}
        hit_error = []
        seam = corners = 0
        forward = unit(sub((-.12,.26,0),eye))
        right = unit((-forward[2],0,forward[0]))
        # Right cross forward yields the world-space camera vertical basis.
        up = (-right[2]*forward[1], right[2]*forward[0]-right[0]*forward[2],right[0]*forward[1])
        fov = math.tan(math.radians(47)*.5)
        ideal_image = list(baseline)
        for i,p in enumerate(position):
            if p[4] != receiver:
                continue
            xy = (i%WIDTH+.5,i//WIDTH+.5)
            for mode,img in capture.items():
                if img[i][0] > .25:
                    capture_masks[mode].add(i)
            ray = unit(add(forward,add(mul(right,(xy[0]*2/WIDTH-1)*fov*WIDTH/HEIGHT),
                                      mul(up,(xy[1]*2/HEIGHT-1)*fov))))
            ideal_direction = None
            distance = intersect(eye,ray,ZERO,1.)
            if distance is not None:
                point = add(eye,mul(ray,distance))
                outgoing = sub(ray,mul(point,2*dot(ray,point)))
                ideal = intersect(point,outgoing,ORANGE,RADIUS)
                if ideal is not None:
                    ideal_direction = outgoing
                    refs["ideal"].add(i)
                    ideal_image[i] = (.87,.18,.02,1,receiver,p[5])
            # Use the actual raster point and corrected normal to test lookup
            # independently of curvature tessellation and horizon correction.
            point = p[:3]
            v = unit(sub(point,eye))
            n = unit(tuple(2*c-1 for c in directions[i][:3]))
            reflected = unit(sub(v,mul(n,2*dot(v,n))))
            if dot(n, v) > -.14:
                continue
            if ideal_direction:
                angles["meshReflectionDirection"].append(angle(ideal_direction,reflected))
            q = box_exit(point,reflected)
            values = {"box": (ZERO,unit(q)), "offset": (OFFSET,unit(sub(q,OFFSET))),
                      "unprojected": (ZERO,reflected)}
            actual = intersect(point,reflected,ORANGE,RADIUS)
            for mode,(origin,d) in values.items():
                margin = sphere_margin(origin,d)
                if margin > 0:
                    refs[mode].add(i)
                if abs(margin)>1.5:
                    g=guarded[mode]
                    g[0] += 1
                    g[1] += (i in capture_masks[mode]) != (margin>0)
                if actual is not None:
                    target = add(point,mul(reflected,actual))
                    angles[mode].append(angle(d,unit(sub(target,origin))))
            ranked = sorted(map(abs,values["box"][1]),reverse=True)
            if abs(sphere_margin(ZERO,values["box"][1])) > 1.5:
                seam += ranked[0]-ranked[1] < .02
                corners += ranked[0]-ranked[2] < .045
            if actual is not None:
                hit = add(point,mul(reflected,actual))
                view = unit(sub(eye,hit))
                facing = dot(view,unit(sub(hit,ORANGE)))
                obstruction = intersect(eye,mul(view,-1),ZERO,1.)
                screen = projection(vp,hit)
                visible = facing > 0 and (obstruction is None or obstruction > math.dist(eye,hit)) and (
                    0 < screen[0] < WIDTH and 0 < screen[1] < HEIGHT)
                if visible:
                    refs["screenVisible"].add(i)
                    if (facing > .35 and sphere_margin(point,reflected)>1.5 and
                            dot(unit(sub(hit,ORANGE)),mul(reflected,-1))>.32 and
                            40<screen[0]<WIDTH-40 and 28<screen[1]<HEIGHT-28 and
                            math.dist(xy,screen[:2])>12):
                        g=guarded["ssr"]
                        g[0] += 1
                        g[1] += i not in capture_masks["ssr"]
                        if trace and trace[i][2]>.05:
                            hit_error.append(math.dist(screen[:2],(trace[i][0]*WIDTH,trace[i][1]*HEIGHT)))
            elif sphere_margin(point,reflected)<-1.5:
                guarded["ssr"][2] += i in capture_masks["ssr"]
        if min(guarded[mode][0] for mode in ("box","offset","unprojected")) < 1000:
            raise RuntimeError("Insufficient guarded sphere lookup samples")
        if any(guarded[m][1]/guarded[m][0]>.001 for m in ("box","unprojected","offset")):
            raise RuntimeError("GPU probe lookup departed from the guarded FP64 model")
        if not guarded["ssr"][0] or guarded["ssr"][1] or guarded["ssr"][2]:
            raise RuntimeError("SSR lost or invented a guarded sphere hit")
        entry = dict(pose=pose, receiver=receiver, edgeDirections=seam, cornerDirections=corners,
                     guarded=guarded, pixels={m: len(v) for m,v in {**refs,**capture_masks}.items()},
                     idealPixels=len(refs["ideal"]),
                     mixedSourceOnlyPixels={
                         mode: len(capture_masks[mode]-refs["ideal"])
                         for mode in ("mixed","mixed-offset","mixed-unprojected")})
        entry["angularErrorP95Degrees"] = {mode: sorted(vals)[int(.95*len(vals))] if vals else None
                                           for mode,vals in angles.items()}
        entry["gpuAgainstLookupContourPixels"] = {name: contour_error(capture_masks[name],refs[name])
                                                  for name in ("box","offset","unprojected")}
        entry["againstIdealContourPixels"] = {name: contour_error(capture_masks[name],refs["ideal"])
                                             for name in ("box","offset","ssr","mixed","unprojected","mixed-offset","mixed-unprojected")}
        entry["centroids"] = {name: centroid([(i%WIDTH,i//WIDTH) for i in mask])
                              for name,mask in (("ideal",refs["ideal"]),*capture_masks.items())}
        if any(value is None or value["p95"]>1.5 for value in entry["gpuAgainstLookupContourPixels"].values()):
            raise RuntimeError("Unexpected contour jump from cube sampling")
        if trace:
            entry["screenHitErrorPixels"] = dict(samples=len(hit_error),
                p95=sorted(hit_error)[int(.95*len(hit_error))] if hit_error else None)
            if not hit_error or entry["screenHitErrorPixels"]["p95"]>2.5:
                raise RuntimeError("SSR accepted screen hits too far from the FP64 projection")
            supported={i for i,p in enumerate(position) if p[4]==receiver and trace[i][2]>.05}
            entry["visibleReferencePixels"]=len(refs["screenVisible"])
            entry["transitionPixels"]=sum(.05<trace[i][2]<.95 for i in supported)
            entry["sourceContoursDisagreeWithinSupport"]=len(
                supported.intersection(capture_masks["box"] ^ refs["ideal"]))
        temporal=folder/f"temporal-{pose}.bin"
        if temporal.exists():
            taa=pixels(temporal)
            mask={i for i,p in enumerate(taa) if p[4]==receiver and p[0]>.25}
            entry["taaAgainstAAOffContourPixels"]=contour_error(mask,capture_masks["mixed"])
        if pose == POSES//2:
            png(destination/"ideal.png",WIDTH,HEIGHT,ideal_image)
            for name,img in capture.items():
                png(destination/f"{name}.png",WIDTH,HEIGHT,img)
            for name in ("global","satin","temporal"):
                name_file=folder/f"{name}-{pose}.bin"
                if name_file.exists():
                    png(destination/f"{name}.png",WIDTH,HEIGHT,pixels(name_file))
            if trace:
                png(destination/"trace.png",WIDTH,HEIGHT,trace)
            outline = {y*WIDTH+x for x,y in contour({i for i,p in enumerate(position) if p[4]==receiver})}
            outline_color = (.045,.06,.07,1,-1,1)
            # An outlined receiver identifies the convex mirror in the crop
            # while leaving the native captures and their black regions intact.
            panels = (ideal_image,capture["box"],capture["mixed-unprojected"],capture["mixed-offset"])
            tile_width,tile_height=100,78
            pixels_out=[]
            for y in range(94,94+tile_height):
                for panel in panels:
                    for x in range(130,130+tile_width):
                        i=y*WIDTH+x
                        pixels_out.append(outline_color if i in outline else panel[i])
            png(destination/"receiver-comparison.png",tile_width*len(panels),tile_height,pixels_out)
        results.append(entry)
    movement={}
    for mode in ("ideal","box","unprojected","offset","ssr","mixed","mixed-offset","mixed-unprojected"):
        points = [row["centroids"][mode] for row in results]
        velocities = [sub(b,a) for a,b in zip(points,points[1:]) if a is not None and b is not None]
        movement[mode] = dict(maxStep=max((math.hypot(*v) for v in velocities),default=None),
                              maxStepChange=max((math.dist(a,b) for a,b in zip(velocities,velocities[1:])),default=None))
    return dict(poses=results, centroidMotionPixels=movement)


def ray_box(origin, direction, low, high):
    enter, leave = -math.inf, math.inf
    for p, v, a, b in zip(origin, direction, low, high):
        if abs(v) < 1e-14:
            if not a <= p <= b:
                return None
            continue
        t0, t1 = (a-p)/v, (b-p)/v
        enter = max(enter, min(t0,t1))
        leave = min(leave, max(t0,t1))
    if leave < enter or leave <= 1e-6:
        return None
    return enter if enter > 1e-6 else leave


def workspace_oracle(folder):
    width,height=2022,748
    eye=tuple(map(float,next(csv.reader((folder/"eye.csv").open()))[1:]))
    objects={}
    for uuid,pick,shape,*bounds in csv.reader((folder/"geometry.csv").open()):
        low,high=tuple(map(float,bounds[:3])),tuple(map(float,bounds[3:]))
        objects[int(uuid)]=(int(pick),shape,low,high)
    receiver=objects[106][0]
    probes=[]
    for _,*row in csv.reader((folder/"probes.csv").open()):
        p,extent,offset,proxy,b=(tuple(map(float,row[:3])),tuple(map(float,row[3:6])),
                                tuple(map(float,row[6:9])),tuple(map(float,row[9:12])),float(row[12]))
        probes.append((p,extent,add(p,offset),proxy,b))

    def nearest(p,r,exclude=None):
        first,entity=math.inf,0
        for uuid,(_,shape,lo,hi) in objects.items():
            if uuid==exclude:
                continue
            if shape=="sphere":
                t=intersect(p,r,mul(add(lo,hi),.5),(hi[0]-lo[0])*.5)
            else:
                t=ray_box(p,r,lo,hi)
            if t is not None and t<first:
                first,entity=t,uuid
        return entity

    rgba=struct.Struct("<4fif")
    world=(folder/"position.bin").read_bytes()
    normals=(folder/"normal.bin").read_bytes()
    actual_views={name:(folder/f"{name}.bin").read_bytes()
                  for name in ("global","local","ssr","mixed","unprojected")}
    oracle,proxy=Counter(),Counter()
    disagreements=0
    differences={k:[] for k in ("global","unprojected","mixed")}
    swatches={0:(.012,.012,.012), 106:(.67,.04,.48), 107:(.78,.19,.018),
              108:(.14,.49,.03),109:(.87,.59,.04),101:(.075,.28,.57),
              102:(.58,.19,.03),103:(.14,.28,.39),104:(.2,.62,.72),105:(.32,.36,.61)}
    match_mask=[]
    for y in range(0,height,3):
        for x in range(0,width,3):
            start=(y*width+x)*rgba.size
            p=rgba.unpack_from(world,start)
            if p[4]!=receiver:
                continue
            n=unit(tuple(2*c-1 for c in rgba.unpack_from(normals,start)[:3]))
            p=p[:3]
            v=unit(sub(p,eye))
            if dot(n,v)>-.2:
                continue
            r=unit(sub(v,mul(n,2*dot(v,n))))
            influence=[]
            for c,extent,pc,proxy_extent,blend in probes:
                u=max(0,min(1,min(e-abs(a-b) for e,a,b in zip(extent,p,c))/blend))
                if u>0:
                    influence.append((u*u*(3-2*u),c,pc,proxy_extent))
            if not influence:
                continue
            weight,c,pc,proxy_extent=max(influence)
            # Use the dominant, at-least-half-strength probe for source attribution.
            if weight<.5:
                continue
            t=ray_box(p,r,sub(pc,proxy_extent),add(pc,proxy_extent))
            if t is None:
                raise RuntimeError("Owner's sphere left the containing projection box")
            d=unit(sub(add(p,mul(r,t)),c))
            reference=nearest(p,r,106)
            sampled=nearest(c,d)
            oracle[reference]+=1
            proxy[sampled]+=1
            disagreements+=reference!=sampled
            observed=rgba.unpack_from(actual_views["local"],start)
            for mode,values in differences.items():
                other=rgba.unpack_from(actual_views[mode],start)
                values.append(max(abs(observed[i]-other[i]) for i in range(3)))
            match_mask.append((x,y,reference,sampled))
    if not match_mask:
        raise RuntimeError("Owner's polished sphere was outside the camera or probe influence")
    xmin,xmax=min(v[0] for v in match_mask),max(v[0] for v in match_mask)
    ymin,ymax=min(v[1] for v in match_mask),max(v[1] for v in match_mask)
    w,h=(xmax-xmin)//3+1,(ymax-ymin)//3+1
    outline=[(0,0,0,1,-1,1)]*(w*2*h)
    for x,y,a,b in match_mask:
        i=(y-ymin)//3*w*2+(x-xmin)//3
        outline[i]=(*swatches.get(a,(.48,.48,.48)),1,-1,1)
        outline[i+w]=(*swatches.get(b,(.48,.48,.48)),1,-1,1)
    png(folder/"probe-source-ids.png",w*2,h,outline)
    return dict(samples=len(match_mask),sphereUUID=106, idealSources=oracle, probeSources=proxy,
                sourceDisagreements=disagreements,
                probeAgainstVariantsP95={name:sorted(vals)[int(.95*len(vals))]
                                         for name,vals in differences.items()})


def diagnostic_resources(resources, destination, config):
    from macos.metal_shaders import produce
    shutil.copytree(resources, destination)
    shader=destination/"shaders/ScreenReflections.glsl"
    source=shader.read_text()
    marker="    float rawDepth = texelFetch(u_Depth, pixel, 0).r;"
    output="    o_Color.rgb = max(radiance.rgb + (confidence * support) * (replacement - fallback), vec3(0));"
    if source.count(marker)!=1 or source.count(output)!=1:
        raise RuntimeError("Trace instrumentation needs the qualified SSR resolve layout")
    shader.write_text(source.replace(marker,"    o_Color.rgb = vec3(0);\n"+marker).replace(output,
        "    o_Color.rgb = vec3(hit / extent, confidence * support);\n"
        "    if (u_Camera.z > 1) o_Color.rgb += replacement - fallback;"))
    shader=destination/"shaders/WorldPresentation.glsl"
    source=shader.read_text()
    output="neutral(world * exp2(gain))"
    if source.count(output)!=1:
        raise RuntimeError("Trace instrumentation needs the qualified HDR presentation layout")
    shader.write_text(source.replace(output,"world * exp2(gain)"))
    produce(config,destination/"shaders/metal",sources=destination/"shaders")
    return destination


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gallery", type=Path, required=True, help="An existing generated Reflection or SSR Gallery")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--config", choices=("Release","Debug"), default="Release")
    parser.add_argument("--resources", type=Path, default=RESOURCES)
    trace=parser.add_mutually_exclusive_group()
    trace.add_argument("--trace", action="store_true", help="Build private shaders that record SSR hit UV and confidence")
    trace.add_argument("--trace-resources", type=Path, help="Reuse the private SSR hit-visualization shader tree")
    parser.add_argument("--workspace", type=Path, help="Compare the saved scene and orbit camera instead of the synthetic fixture")
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True)
    original = args.gallery.resolve()
    project_folder = out/"Sphere Gallery"
    shutil.copytree(original.parent, project_folder, ignore=shutil.ignore_patterns("*.lock","*.log"))
    project = project_folder/original.name
    if not args.workspace:
        measured = project_folder/"Assets/SphereStudy"
        measured.mkdir()
    resources = args.resources.resolve()
    if args.trace:
        args.trace_resources=diagnostic_resources(resources,out/"Trace Resources",args.config)
    env = {k:v for k,v in os.environ.items() if not k.startswith(("HAZEL_","DYLD_","MTL_"))}
    env.update(HAZEL_RENDERING_REFERENCE_PROJECT=str(project), HAZEL_REFLECTION_DIAGNOSTIC="1",
               DYLD_LIBRARY_PATH=str(hz.mono_prefix()/"lib"), MTL_DEBUG_LAYER="1", MTL_SHADER_VALIDATION="1")

    if args.workspace:
        pose=out/"orbit.yaml"
        shutil.copyfile(args.workspace.resolve(),pose)
        env["HAZEL_REFLECTION_WORKSPACE"]=str(pose)

    def run(api, mode, root=resources, tag=""):
        folder = out/f"{api}-{mode}{tag}"
        with (out/f"{folder.name}.log").open("w") as log:
            subprocess.run(native_command(args.config,api,root,folder),cwd=hz.ROOT,timeout=150,check=True,
                           stdout=log,stderr=subprocess.STDOUT,env={**env,"HAZEL_REFLECTION_SPHERES":mode,
                           "HAZEL_DATA":str(out/f"{api}-profile{tag}"),"HAZEL_RESOURCES":str(root)})
        return folder

    result = dict(config=args.config,head=hz.output(["git","rev-parse","HEAD"]),cube={},viewport={})
    if args.workspace:
        for api in ("metal","opengl"):
            folder=run(api,"workspace")
            for name in ("global","local","ssr","mixed","unprojected","ibl-only","direct-only"):
                image=pixels(folder/f"{name}.bin")
                png(folder/f"{name}.png",2022,748,image)
            result["viewport"][api]=workspace_oracle(folder)
            result["viewport"][api]["uniformIBLGrazing"]=next(csv.DictReader((folder/"halo.csv").open()))
            if args.trace_resources:
                folder=run(api,"workspace",args.trace_resources.resolve(),"-trace")
                png(folder/"trace.png",2022,748,pixels(folder/"ssr.bin"))
        (out/"results.json").write_text(json.dumps(result,indent=2)+"\n")
        return
    captures = {api:run(api,"capture") for api in ("metal","opengl")}
    for api,folder in captures.items():
        result["cube"][api] = {name:cube_oracle(folder/f"{name}.hlight",p,folder/name)
                               for name,p in (("center",ZERO),("offset",OFFSET))}
    for name in ("center","offset"):
        raw = captures["metal"]/f"{name}.hlight"
        shutil.copy2(raw,measured/raw.name)
        descriptor = measured/f"{name}.henv"
        descriptor.write_text(f"""Environment:
  Version: 1
  Name: Sphere study {name}
  Notice: Procedural Hazel qualification fixture, Apache-2.0
  Source:
    Kind: SceneCapture
    Image: SphereStudy/{name}.hlight
    Interpretation: LinearRec709D65Cube
  Preparation:
    Recipe: Hazel.IBL_v1.GGX-correlated-single.r045.E-pi.AB-NoV-r.cube-top
    Background: 16
    Specular: {SIDE}
    Irradiance: 32
    Samples: 1024
""")
        tool = hz.binaries(args.config)/"HazelProject/HazelProject"
        with (out/f"prepare-{name}.log").open("w") as log:
            subprocess.run([tool,"prepare-environment",project,f"SphereStudy/{name}.henv"],
                           stdout=log,stderr=subprocess.STDOUT,check=True,timeout=120,env=env)
        mip = next(measured.glob(f"{name}.henv.prepared/*/Specular.hlight"))
        if raw.read_bytes()[72:] != mip.read_bytes()[72:72+SIDE*SIDE*6*12]:
            raise RuntimeError("Prepared mip zero differs from the capture")

    for api in ("metal","opengl"):
        folder = run(api,"compare")
        diagnostic = run(api,"compare",args.trace_resources.resolve(),"-trace") if args.trace_resources else None
        result["viewport"][api] = verify(folder,folder,diagnostic)
    parity = []
    for pose in range(POSES):
        for mode in MODES:
            first,second = (pixels(out/f"{api}-compare"/f"{mode}-{pose}.bin") for api in ("metal","opengl"))
            dif = sorted(max(abs(a[k]-b[k]) for k in range(3))
                         for a,b in zip(first,second) if a[4]==b[4] and a[4]!=-1)
            ids = sum(a[4]!=b[4] for a,b in zip(first,second))
            p99 = dif[int(.99*len(dif))]
            if p99>.015 or ids>460:
                raise RuntimeError(f"Sphere study native parity exceeded the established Gallery bounds: {mode} {pose}")
            parity.append(dict(pose=pose,mode=mode,rgbP99=p99,idDifferences=ids))
    result["parity"]=parity
    (out/"results.json").write_text(json.dumps(result,indent=2)+"\n")


if __name__ == "__main__":
    main()
