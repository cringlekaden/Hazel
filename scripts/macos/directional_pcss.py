#!/usr/bin/env python3
"""Native directional PCSS captures and independent FP64 parallel-plane references."""
import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import statistics
import struct
import subprocess
import sys
import resource
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import hazel as hz
from macos.xegtao import pixels,png

PIXEL=struct.Struct('<4fif')
DISK=[]
for y in range(4):
    for x in range(4):
        a=(x+.5)/2-1;b=(y+.5)/2-1
        radius,angle=(a,math.pi/4*(b/a)) if abs(a)>abs(b) else (b,math.pi/2-math.pi/4*(a/b))
        DISK.append((radius*math.cos(angle),radius*math.sin(angle)))


def point(blob,index):return PIXEL.unpack_from(blob,index*24)
def f32(x):return struct.unpack('<f',struct.pack('<f',x))[0]
def ulp(x):return 2.**(math.frexp(abs(x))[1]-24) if x else 2.**-149
def clip(matrix,p):return [sum(matrix[c*4+r]*p[c] for c in range(4))*.5+.5 for r in range(3)]


def continuous_disk(x,r):
    if r==0:return float(x>=0)
    t=max(-1.,min(1.,x/r));return .5+(math.asin(t)+t*math.sqrt(max(0,1-t*t)))/math.pi


def crossing(samples,level):
    for (x,a),(y,b) in zip(samples,samples[1:]):
        if a<level<=b:return x+(y-x)*(level-a)/(b-a)
    return None


def reference(position,meta,matrix,candidate=None):
    """Ray-intersect a half-plane blocker at actual texel centres, then integrate.

    This does not read GPU shadow depths or call a production shadow helper.
    Surface y=0; light=+Y; blocker occupies x<0 at the authored height.
    """
    candidate=candidate or {};search_count=candidate.get('blockerLocations',16);filter_count=candidate.get('filterLocations',16)
    search_points=DISK if search_count==16 else [(0.,0.)]+[(math.sqrt((i+.5)/(search_count-1))*math.cos(i*math.pi*(3-math.sqrt(5))),math.sqrt((i+.5)/(search_count-1))*math.sin(i*math.pi*(3-math.sqrt(5)))) for i in range(search_count-1)]
    filter_points=DISK if candidate.get('filterPattern','concentric')=='concentric' else [(math.sqrt((i+.5)/filter_count)*math.cos(i*math.pi*(3-math.sqrt(5))),math.sqrt((i+.5)/filter_count)*math.sin(i*math.pi*(3-math.sqrt(5)))) for i in range(filter_count)]
    resolution=int(meta['resolution']);texel=float(meta['texel']);span=float(meta['span']);height=float(meta['height']);angle=float(meta['angle']);hi=float(meta['searchHi']);lx=float(meta.get('lightX',0));ly=float(meta.get('lightY',1))
    right=[matrix[i] for i in [0,4,8]];up=[matrix[i] for i in [1,5,9]];rl=math.sqrt(sum(x*x for x in right));ul=math.sqrt(sum(x*x for x in up));right=[x/rl for x in right];up=[x/ul for x in up];slope=[max(-8,min(8,right[1]/ly)),max(-8,min(8,up[1]/ly))]
    p=clip(matrix,(*position,1));bias=.05*texel/span
    fused_contact=False;fused_radius=0.
    def stored(tap):
        a=(2*(tap[0]+.5)/resolution-1-matrix[12])/rl;b=(2*(tap[1]+.5)/resolution-1-matrix[13])/ul
        base=[right[i]*a+up[i]*b for i in range(3)]
        def ray(h):
            t=(h-base[1])/ly;return [base[i]+(lx if i==0 else ly if i==1 else 0)*t for i in range(3)]
        world=ray(0.);upper=ray(height)
        offset=height*lx/ly if 'width' in meta else 0.
        inside=abs(upper[0]-offset)<float(meta['width'])/2 if 'width' in meta else upper[0]<0
        edge=abs(abs(upper[0]-offset)-float(meta['width'])/2) if 'width' in meta else abs(upper[0])
        if inside:world=upper
        if meta.get('layers')=='1':
            h=float(meta.get('lowerHeight',1));lower=ray(h);centre=float(meta['width'])*.5+h*lx/ly;extent=float(meta['width'])*.375
            if not inside and abs(lower[0]-centre)<extent:world=lower
            edge=min(edge,abs(abs(lower[0]-centre)-extent))
        z=f32(clip(matrix,(*world,1))[2]);return z,edge<=texel/(16*abs(ly))
    def receiver(tap):return p[2]+sum(slope[k]*((tap[k]+.5-p[k]*resolution)*texel) for k in range(2))/span
    def comparison(delta):
        coordinate=[p[j]*resolution-.5+delta[j]/texel for j in range(2)];cell=[math.floor(x) for x in coordinate];fraction=[x-y for x,y in zip(coordinate,cell)];value=0.;ambiguous=False
        for j in range(2):
            for i in range(2):
                tap=[max(0,min(resolution-1,cell[k]+(i if k==0 else j))) for k in range(2)];depth,edge=stored(tap);ambiguous|=edge
                compare=receiver(tap)-bias
                if candidate.get('comparison')=='native plane envelope' and not fused_contact:
                    envelope=sum(abs(slope[k])*max(fraction[k],1-fraction[k])*texel for k in range(2))
                    if candidate.get('fusedFilter') and all((fused_radius+texel)/(texel*resolution)<=p[k]<=1-(fused_radius+texel)/(texel*resolution) for k in range(2)):envelope=sum(abs(s)*texel for s in slope)
                    compare=p[2]+sum(slope[k]*delta[k] for k in range(2))/span-bias-envelope/span
                value+=float(depth>=1 or compare<=depth)*(fraction[0] if i else 1-fraction[0])*(fraction[1] if j else 1-fraction[1])
        return value,ambiguous
    if angle==0:
        fused_contact=bool(candidate.get('fusedFilter'));return (*comparison((0,0)),0.)
    tangent=math.tan(angle);search=min(1.,max(hi-lx*position[0]-ly*position[1],0)*tangent/(1-8*math.sqrt(2)*tangent));blockers=[];ambiguous=False
    if candidate.get('receiverPlaneBounds'):
        light=[lx,ly,0.];plane=[light[i]+right[i]*slope[0]+up[i]*slope[1] for i in range(3)]
        low=[-40.,-.2,-60.];high=[40.,height,20.];corner=[high[i] if plane[i]>=0 else low[i] for i in range(3)]
        separation=max(sum(plane[i]*(corner[i]-position[i]) for i in range(3))/sum(plane[i]*light[i] for i in range(3)),0.)
        search=min(1.,separation*tangent/(1-math.hypot(*slope)*tangent))
    for index,delta in enumerate(search_points):
        delta=(0,0) if search_count==16 and index==5 else delta
        tap=[max(0,min(resolution-1,math.floor(p[k]*resolution+delta[k]*search/texel))) for k in range(2)];depth,edge=stored(tap);ambiguous|=edge
        value=receiver(tap)
        if depth<1 and depth<value-bias:blockers.append(max((value-depth)*span,0))
    weights=None
    if not blockers and candidate.get('refineEmpty'):
        coordinate=[p[k]*resolution-.5 for k in range(2)];cell=[math.floor(x) for x in coordinate];f=[x-y for x,y in zip(coordinate,cell)];weights=[]
        for j in range(2):
            for i in range(2):
                weight=(f[0] if i else 1-f[0])*(f[1] if j else 1-f[1]);tap=[max(0,min(resolution-1,cell[k]+(i if k==0 else j))) for k in range(2)];depth,edge=stored(tap);ambiguous|=edge;value=receiver(tap)
                if weight>0 and depth<1 and depth<value-bias:blockers.append(max((value-depth)*span,0));weights.append(weight)
    if not blockers:return 1.,ambiguous,0.
    radius=min((sum(a*b for a,b in zip(blockers,weights))/sum(weights) if weights else statistics.mean(blockers))*tangent,1.);value=0.
    fused_radius=radius;fused_contact=bool(candidate.get('fusedFilter')) and min(blockers)<=4*sum(abs(s)*texel for s in slope)
    for delta in filter_points:
        result,edge=comparison([d*radius for d in delta]);value+=result/filter_count;ambiguous|=edge
    return value,ambiguous,radius


def inspect(folder,candidate=None):
    metadata=list(csv.DictReader((folder/'planes.csv').open()));projections=list(csv.DictReader((folder/'projections.csv').open()));cases=[]
    for number in sorted({int(m['case']) for m in metadata}):
        m=next(m for m in metadata if int(m['case'])==number and m['cascade']=='0');row=next(x for x in projections if int(x['case'])==number and x['cascade']=='0');matrix=[float(row['m'+str(i)]) for i in range(16)]
        name='plane-'+str(number);pos=(folder/(name+'-position.bin')).read_bytes();vis=(folder/(name+'-visibility.bin')).read_bytes();physical=(folder/(name+'-physical.bin')).read_bytes();pair=(folder/(name+'-pair.bin')).read_bytes();radiance=(folder/(name+'-radiance.bin')).read_bytes();samples=[];errors=[];ambiguous=0;radius_errors=[];separation_errors=[];pair_errors=[];disk_errors=[];radiance_errors=[]
        height=float(m['height']);alpha=float(m['angle']);ly=float(m.get('lightY',1));edge=-height*float(m.get('lightX',0))/ly;radius=min(height/ly*math.tan(alpha),1);texel=float(m['texel']);surface_texel=texel/abs(ly);span=float(m['span']);lit=point(radiance,128*1024+1020)[:3];lost=0;pcf=(folder/(name+'-pcf-visibility.bin')).read_bytes() if (folder/(name+'-pcf-visibility.bin')).exists() else None
        for x in range(1024):
            i=128*1024+x;p=point(pos,i);v=point(vis,i)[0];probe=point(physical,i);maps=point(pair,i)
            if p[4]!=11:raise RuntimeError('Plane lost primary coverage')
            expected,raster_edge,model_radius=reference(p[:3],m,matrix,candidate)
            if raster_edge:ambiguous+=1
            else:errors.append(abs(v-expected))
            if alpha>0 and p[0]<edge-max(radius/abs(ly),surface_texel)*2:
                radius_errors.append(abs(probe[1]-radius));separation_errors.append(abs(probe[0]-height/ly));pair_errors.append(abs(maps[0]-maps[1]))
                if pcf and point(pcf,i)[0]<.02 and v>.02:lost+=1
            if abs(p[0]-edge)<=radius/abs(ly)+2*surface_texel:disk_errors.append(abs(v-continuous_disk((p[0]-edge)*abs(ly),radius)))
            samples.append((p[0],v))
            radiance_errors.extend(abs(point(radiance,i)[c]-lit[c]*v) for c in range(3))
        samples.sort();a,b,c=[crossing(samples,v) for v in [.1,.5,.9]]
        t0,t1=0.,1.
        for _ in range(60):
            mid=(t0+t1)*.5
            if continuous_disk(mid,1)<.9:t0=mid
            else:t1=mid
        expected_width=2*(t0+t1)*.5*radius/abs(ly);bound=8*ulp(.5)*span+1e-5
        record=dict(case=number,metadata=m,radius=radius,texel=texel,resolved=radius>=4*texel,contour50=b,width10to90=c-a if a is not None and c is not None else None,continuousDiskWidth=expected_width,maxFP64VisibilityError=max(errors) if errors else None,unambiguousProbes=len(errors),ambiguousProbes=ambiguous,maxSeparationError=max(separation_errors,default=0),separationBound=bound,maxRadiusError=max(radius_errors,default=0),radiusBound=math.tan(alpha)*bound+1e-5,maxPairRadiusDifference=max(pair_errors,default=0),meanDiskVisibilityError=statistics.mean(disk_errors) if disk_errors else 0,maxDiskVisibilityError=max(disk_errors,default=0),maxRadianceVisibilityError=max(radiance_errors),failures=[])
        plateau=[x for x,v in samples if abs(v-.5)<1e-7]
        record['halfVisibilityPlateau']=dict(minimum=min(plateau),maximum=max(plateau),midpoint=(min(plateau)+max(plateau))*.5) if len(plateau)>1 else None
        record.update(analyticEdge=edge,receiverTexel=surface_texel,lostFullyBlockedProbes=lost)
        f=record['failures']
        if record['maxFP64VisibilityError'] is None:f.append('reference entirely raster-ambiguous')
        elif record['maxFP64VisibilityError']>.02:f.append('FP64 quadrature visibility')
        if record['maxSeparationError']>bound:f.append('decoded separation')
        if record['maxRadiusError']>record['radiusBound']:f.append('physical radius')
        if record['maxPairRadiusDifference']>.01*radius+2*record['radiusBound']:f.append('independent cascade radii')
        if b is None or abs(b-edge)>surface_texel:f.append('50% contour/contact')
        if lost:f.append('new lost contact/fully blocked shadow versus PCF')
        if record['resolved']:
            if record['width10to90'] is None or abs(record['width10to90']/expected_width-1)>.2:f.append('physical penumbra width')
            if record['meanDiskVisibilityError']>.035 or record['maxDiskVisibilityError']>.1:f.append('continuous disk quadrature')
        if record['maxRadianceVisibilityError']>.003:f.append('rendered direct radiance')
        record['status']='FAIL' if f else 'PASS scoped plane'
        cases.append(record);png(folder/(name+'.png'),1024,256,pixels(folder/(name+'-visibility.bin')))
    repeat=(folder/'plane-0-repeat.bin').read_bytes()==(folder/'plane-0-visibility.bin').read_bytes()
    result=dict(status='PASS scoped planes' if repeat and not any(c['failures'] for c in cases) else 'FAIL',staticRepeat=repeat,cases=cases)
    if (folder/'clear-visibility.bin').exists():
        clear=[p for p in PIXEL.iter_unpack((folder/'clear-visibility.bin').read_bytes())]
        probes=[p for p in PIXEL.iter_unpack((folder/'clear-physical.bin').read_bytes())]
        result['clearAtlas']=dict(maxVisibilityError=max(abs(p[0]-1) for p in clear),maximumBlockerFraction=max(p[2] for p in probes))
    (folder/'planes-results.json').write_text(json.dumps(result,indent=2)+'\n');return result


def inspect_strips(folder,candidate):
    cases=[]
    for meta in csv.DictReader((folder/'strips.csv').open()):
        n=meta['case'];matrix=[float(meta['m'+str(i)]) for i in range(16)];pos=(folder/('strip-'+n+'-position.bin')).read_bytes();vis=(folder/('strip-'+n+'-visibility.bin')).read_bytes();control=(folder/('strip-'+n+'-pcf.bin')).read_bytes()
        width=float(meta['width']);radius=float(meta['height'])*math.tan(float(meta['angle']));tx=float(meta['texel']);errors=[];disk_errors=[];ambiguous=0;lost=0;umbra=0;tails=0
        for x in range(1024):
            i=128*1024+x;p=point(pos,i);v=point(vis,i)[0];expected,uncertain,_=reference(p[:3],meta,matrix,candidate)
            if uncertain:ambiguous+=1
            else:errors.append(abs(v-expected))
            if meta['layers']=='0':
                truth=1-(continuous_disk(p[0]+width*.5,radius)-continuous_disk(p[0]-width*.5,radius))
                if abs(p[0])<=width*.5+radius+2*tx:disk_errors.append(abs(v-truth))
                if truth<.9 and v>.999:tails+=1
                if abs(p[0])<width*.5-radius-2*tx and point(control,i)[0]<.02:
                    umbra+=1;lost+=v>.02
        record=dict(metadata=meta,maxFP64VisibilityError=max(errors,default=None),unambiguousProbes=len(errors),ambiguousProbes=ambiguous,stableFullUmbraProbes=umbra,newLostFullUmbra=lost,partialTailLitProbes=tails,maxContinuousDiskError=max(disk_errors,default=None),meanContinuousDiskError=statistics.mean(disk_errors) if disk_errors else None)
        record['status']='PASS finite depth-grid reference and tested umbra' if errors and max(errors)<=.02 and lost==0 else 'FAIL'
        cases.append(record);png(folder/('strip-'+n+'.png'),1024,256,pixels(folder/('strip-'+n+'-visibility.bin')))
    result=dict(status='FAIL' if any(r['status']=='FAIL' for r in cases) else 'PASS scoped thin/layered depth-grid',cases=cases,scope='Finite strip oracle uses independent FP64 ray intersections/native depth quantization; no GPU depth read. Continuous disk errors are reported separately for overlapping/subtexel edges, not substituted for frozen isolated-edge gates.')
    (folder/'strips-results.json').write_text(json.dumps(result,indent=2)+'\n');return result


def inspect_motion(folder):
    groups={}
    for row in csv.DictReader((folder/'motion.csv').open()):
        n=row['case'];pos=(folder/('motion-'+n+'-position.bin')).read_bytes();vis=(folder/('motion-'+n+'-visibility.bin')).read_bytes()
        samples=sorted((point(pos,16*512+x)[0],point(vis,16*512+x)[0]) for x in range(512))
        key='/'.join(row[k] for k in ('resolution','height','motion','filter'));groups.setdefault(key,[]).append(crossing(samples,.5))
    records={}
    for key,values in groups.items():
        if None in values:records[key]=dict(status='FAIL lost contour');continue
        centre=statistics.median(values);drifts=sorted(abs(v-centre)*512 for v in values)
        records[key]=dict(contours=values,p95DriftPixels=drifts[math.ceil(.95*len(drifts))-1],maxAdjacentPixels=max(abs(a-b)*512 for a,b in zip(values,values[1:])))
    for key,row in records.items():
        if key.endswith('/1') and 'contours' in row:
            control=records[key[:-1]+'0'];row['driftIncreasePixels']=row['p95DriftPixels']-control['p95DriftPixels'];row['stepIncreasePixels']=row['maxAdjacentPixels']-control['maxAdjacentPixels']
            row['status']='PASS scoped temporal' if max(row['driftIncreasePixels'],row['stepIncreasePixels'])<=.25 else 'FAIL temporal'
    result=dict(status='FAIL' if any(r.get('status','').startswith('FAIL') for r in records.values()) else 'PASS scoped temporal',cases=records,scope='512x32 output, world-space50% first crossing at central row;32frames,translation/rotation,TAAoff,PCF controls; pixels convert by512/worldunit')
    (folder/'motion-results.json').write_text(json.dumps(result,indent=2)+'\n');return result


def benchmark(folder):
    groups={}
    for row in csv.DictReader((folder/'overlap.csv').open()):
        key='/'.join(row[k] for k in ('width','height','resolution','filter','depth'));groups.setdefault(key,[]).append(row)
    summary={}
    for key,rows in groups.items():
        summary[key]={}
        for field in ('shadowGPUms','forwardGPUms','serializedMs')+(('totalGPUms',) if 'totalGPUms' in rows[0] else ()):
            values=sorted(float(r[field]) for r in rows)
            summary[key][field]=dict(median=statistics.median(values),p95=values[math.ceil(.95*len(values))-1],samples=len(values))
    return dict(status='MEASURED; acceptance separate',warmups=5,cases=summary,scope='Fully blocked, single/overlap/single, offscreen serialized. Metal GPU atlas/forward counters; GL total SceneRenderer elapsed query where recorded, no GL pass split. Serialized CPU is not GPU timing.')


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--config',choices=('Debug','Release'),default='Release');p.add_argument('--resources',type=Path,default=hz.ROOT/'Hazel/Resources');p.add_argument('--output',type=Path,required=True);p.add_argument('--backend',choices=('metal','opengl','both'),default='both');p.add_argument('--inspect-only',action='store_true');p.add_argument('--benchmark',action='store_true');p.add_argument('--angled',action='store_true',help='45-degree directional light in the overlap benchmark');p.add_argument('--capture-only',action='store_true');p.add_argument('--authored',action='store_true');p.add_argument('--motion',action='store_true');p.add_argument('--strips',action='store_true');p.add_argument('--pcf-control',action='store_true');p.add_argument('--layered',action='store_true');p.add_argument('--retina',action='store_true');p.add_argument('--focused',action='store_true',help='Only1080p overlap,1024/2048,PCSS rows');p.add_argument('--caster-height',type=float);p.add_argument('--angular-radius',type=float);p.add_argument('--executables',type=Path,help='Explicit isolated prototype binaries; otherwise use the current build');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
    if not a.inspect_only and any(out.iterdir()):raise RuntimeError('Use a fresh capture directory')
    resource_source=(a.resources/'shaders/ForwardPBR.glsl').read_text()
    if 'softShadow(' not in resource_source:
        raise RuntimeError('Production retains PCF. Select archived PCSS resources and matching --executables, or explicitly apply/rebuild the recorded prototype first.')
    engine_source=(hz.ROOT/'Hazel/Resources/shaders/ForwardPBR.glsl').read_text()
    if not a.inspect_only and 'softShadow(' not in engine_source and a.executables is None:
        raise RuntimeError('The current production binaries do not implement PCSS; --executables must name the matching isolated prototype binaries.')
    env=os.environ.copy();env.update(DYLD_LIBRARY_PATH=str(hz.mono_prefix()/'lib'),HAZEL_RESOURCES=str(a.resources.resolve()),HAZEL_RENDERING_REFERENCE_PROJECT=str(hz.ROOT/'examples/RenderingReference/RenderingReference.hproj'),HAZEL_DATA=str(out/'profile'),HAZEL_PCSS_QUALIFY='1',MTL_DEBUG_LAYER='1',MTL_SHADER_VALIDATION='1')
    if a.benchmark:env.update(HAZEL_PCSS_BENCHMARK='1');env.pop('MTL_DEBUG_LAYER',None);env.pop('MTL_SHADER_VALIDATION',None)
    if a.angled:env['HAZEL_PCSS_ANGLED_BENCHMARK']='1'
    else:env.pop('HAZEL_PCSS_ANGLED_BENCHMARK',None)
    if a.focused:env['HAZEL_PCSS_FOCUSED_BENCHMARK']='1'
    if a.retina:env['HAZEL_PCSS_RETINA_BENCHMARK']='1'
    if a.layered:env['HAZEL_PCSS_LAYERED_BENCHMARK']='1'
    if a.authored:env['HAZEL_PCSS_AUTHORED']='1';a.capture_only=True
    if a.motion:env['HAZEL_PCSS_MOTION']='1';a.capture_only=True
    if a.strips:env['HAZEL_PCSS_STRIPS']='1';a.capture_only=True
    if a.pcf_control:env['HAZEL_PCSS_PCF_CONTROL']='1'
    if a.caster_height is not None:env['HAZEL_PCSS_BENCHMARK_HEIGHT']=str(a.caster_height)
    if a.angular_radius is not None:env['HAZEL_PCSS_BENCHMARK_ANGLE']=str(a.angular_radius)
    report=dict(configuration=a.config,head=hz.output(['git','rev-parse','HEAD']),sourceSHA256=hashlib.sha256((a.resources/'shaders/ForwardPBR.glsl').read_bytes()).hexdigest(),commands={},backends={},angled=a.angled,casterHeight=a.caster_height,angularRadius=a.angular_radius,status='INCOMPLETE')
    if '#define HZ_DIRECTIONAL_PCSS 0' in (a.resources/'shaders/ForwardPBR.glsl').read_text():
        report['candidate']=dict(blockerLocations=8,filterLocations=32,filterPattern='vogel',comparison='native plane envelope',fusedFilter=True,smallContact=True,refineEmpty=True,productionSelected=False,integratedCandidate=True,receiverPlaneBounds='u_ShadowCasterMinimum' in (a.resources/'shaders/ForwardPBR.glsl').read_text())
    if (a.resources/'candidate.json').exists():
        report['candidate']=json.loads((a.resources/'candidate.json').read_text())
        if report['candidate'].get('shadeOnly'):
            report['baselineWarning']='Specialized PCSS: CPU filter=0 rows use a point limit, not production PCF. Compare filter=1 rows against a separate unspecialized PCF run.'
            env['HAZEL_PCSS_SHADE_ONLY']='1'
            if not a.benchmark and not a.capture_only and not a.inspect_only:raise RuntimeError('Specialized shading requires --capture-only; qualify diagnostics through its full inspection twin')
    try:
        for backend in (('metal','opengl') if a.backend=='both' else (a.backend,)):
            binaries=a.executables or hz.binaries(a.config)
            cmd=[binaries/'MetalRenderer2DQuadSmoke/MetalRenderer2DQuadSmoke',a.resources.resolve(),out/backend,'--shadows'] if backend=='metal' else [binaries/'MigrationRenderer2DSmoke/MigrationRenderer2DSmoke','--shadows',out/backend];report['commands'][backend]=list(map(str,cmd))
            if not a.inspect_only:
                with (out/(backend+'.log')).open('w') as log:r=subprocess.run(list(map(str,cmd)),env=env,cwd=hz.ROOT,stdout=log,stderr=subprocess.STDOUT,timeout=600)
                if r.returncode:raise RuntimeError(backend+' capture failed')
            report['backends'][backend]=benchmark(out/backend) if a.benchmark else dict(status='CAPTURED; NOT QUALIFIED') if a.capture_only else inspect(out/backend,report.get('candidate'))
        report['status']='MEASURED; acceptance separate' if a.benchmark else 'CAPTURED; NOT QUALIFIED' if a.capture_only else 'PASS scoped planes' if all(v['status'].startswith('PASS') for v in report['backends'].values()) else 'FAIL'
        report['peakChildResidentBytes']=resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss
    finally:(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    print(report['status'])
    if report['status']=='FAIL':raise SystemExit(1)


if __name__=='__main__':main()
