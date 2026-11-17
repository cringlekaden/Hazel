#!/usr/bin/env python3
"""Independent deterministic-line qualification and preserved legacy model analysis."""
import argparse
import json
import math
from pathlib import Path
import struct
import sys
import zipfile
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))


def clip_segment(a, b, extent, axes=range(3), depths=False):
    lo, hi = 0.0, 1.0
    lo_depth=hi_depth=None
    for axis in axes:
        for sign in (-1, 1):
            x, y = a[3]+sign*a[axis], b[3]+sign*b[axis]
            if x < 0 and y < 0:
                return None
            if x < 0:
                candidate=x/(x-y)
                if candidate>lo:
                    lo=candidate;lo_depth=(0.0 if sign==1 else 1.0) if axis==2 else None
            if y < 0:
                candidate=x/(x-y)
                if candidate<hi:
                    hi=candidate;hi_depth=(0.0 if sign==1 else 1.0) if axis==2 else None
    if lo > hi:
        return None
    def project(t):
        q = [x+t*(y-x) for x,y in zip(a,b)]
        if q[3] <= 0:
            raise RuntimeError('Unprojectable clipped endpoint')
        return tuple((q[i]/q[3]+1)*extent/2 for i in (0,1))
    result=(project(lo), project(hi))
    if depths:
        def depth(t):
            q=[x+t*(y-x) for x,y in zip(a,b)]
            return q[2]/q[3]*.5+.5
        return (*result,depth(lo) if lo_depth is None else lo_depth,depth(hi) if hi_depth is None else hi_depth)
    return result


def rectangle(a, b, width):
    dx, dy = b[0]-a[0], b[1]-a[1]
    length = math.hypot(dx, dy)
    if length == 0:
        return []
    nx, ny = -dy/length*width/2, dx/length*width/2
    return [(a[0]+nx,a[1]+ny),(b[0]+nx,b[1]+ny),
            (b[0]-nx,b[1]-ny),(a[0]-nx,a[1]-ny)]


def cut(polygon, axis, bound, sign):
    if not polygon:
        return []
    result = []
    previous = polygon[-1]
    before = (previous[axis]-bound)*sign
    for point in polygon:
        after = (point[axis]-bound)*sign
        if (before >= 0) != (after >= 0):
            t = before/(before-after)
            result.append(tuple(p+t*(q-p) for p,q in zip(previous,point)))
        if after >= 0:
            result.append(point)
        previous, before = point, after
    return result


def pixel_area(polygon, x, y):
    for axis,bound,sign in ((0,x,1),(0,x+1,-1),(1,y,1),(1,y+1,-1)):
        polygon = cut(polygon,axis,bound,sign)
    if not polygon:
        return 0.0
    return min(1.0,abs(sum(p[0]*polygon[(i+1)%len(polygon)][1]-p[1]*polygon[(i+1)%len(polygon)][0]
                           for i,p in enumerate(polygon)))/2)


def coverage_ramp(a, b, width, x, y):
    dx,dy=b[0]-a[0],b[1]-a[1]
    length=math.hypot(dx,dy)
    if not length:
        return 0.0
    px,py=x+.5-(a[0]+b[0])/2,y+.5-(a[1]+b[1])/2
    qx=abs((px*dx+py*dy)/length)-length/2
    qy=abs((-px*dy+py*dx)/length)-width/2
    distance=math.hypot(max(qx,0),max(qy,0))+min(max(qx,qy),0)
    return max(0.0,min(1.0,.5-distance))


def coverage_major_axis(a, b, width, x, y):
    major=0 if abs(b[0]-a[0]) >= abs(b[1]-a[1]) else 1
    minor=1-major
    if a[major] > b[major]:
        a,b=b,a
    if a[major] == b[major]:
        return 0.0
    pixel=(x,y)
    if not math.ceil(a[major]-.5) <= pixel[major] < math.ceil(b[major]-.5):
        return 0.0
    center=a[minor]+(pixel[major]+.5-a[major])*(b[minor]-a[minor])/(b[major]-a[major])
    return max(0.0,min(1.0,(width+1)/2-abs(pixel[minor]+.5-center)))


def analyze(directory):
    metadata=json.loads((directory/'cases.json').read_text())
    measurements=[]
    for case in metadata['cases']:
        extent=case['extent']
        projected=clip_segment(case['clipA'],case['clipB'],extent)
        if projected is None:
            a=b=(0,0)
        else:
            a,b=projected
        polygon=rectangle(a,b,case['effectiveWidth'])
        models={name:dict(maxColorError=0.0,idMismatches=0,colorFailures=0,coverageSum=0.0)
                for name in ('pixelArea','distanceRamp','majorAxisRamp')}
        row=dict(case=case['file'],name=case['name'],extent=extent,
                 requestedWidth=case['requestedWidth'],effectiveWidth=case['effectiveWidth'],
                 projectedA=a,projectedB=b,glPixels=0,zeroAlphaIDs=0,glCoverageSum=0.0,models=models)
        data=(directory/case['file']).read_bytes()
        if len(data)!=extent*extent*20:
            raise RuntimeError('Invalid readback length')
        for i,pixel in enumerate(struct.iter_unpack('<4fi',data)):
            if not all(math.isfinite(v) for v in pixel[:4]):
                raise RuntimeError('Nonfinite reference color')
            if pixel[4] not in (-1,case['id']):
                raise RuntimeError('Unexpected signed entity ID (including passive overlap)')
            x,y=i%extent,i//extent
            clip=case['clip']
            inside=clip[2]<0 or (clip[0]<=x+.5<clip[0]+clip[2] and clip[1]<=y+.5<clip[1]+clip[3])
            observed=pixel[3]
            row['glPixels']+=pixel[4]==case['id']
            row['zeroAlphaIDs']+=pixel[4]==case['id'] and observed==0
            row['glCoverageSum']+=(1-max(0,1-observed)**(1/case['repeats']))/case['alpha']
            coverage={'pixelArea':pixel_area(polygon,x,y),
                      'distanceRamp':coverage_ramp(a,b,case['effectiveWidth'],x,y),
                      'majorAxisRamp':coverage_major_axis(a,b,case['effectiveWidth'],x,y)}
            for name,value in coverage.items():
                value=value if inside else 0.0
                expected=1-(1-case['alpha']*value)**case['repeats']
                error=max(abs(expected-channel) for channel in pixel[:4])
                model=models[name]
                model['maxColorError']=max(model['maxColorError'],error)
                model['colorFailures']+=error>2e-5
                # Explicit visible-coverage picking candidate; legacy also writes
                # IDs at some exactly-zero-alpha fringe pixels. No epsilon is
                # applied to the reference or to an acceptance comparison.
                model['idMismatches']+=(value>1e-12)!=(pixel[4]==case['id'])
                model['coverageSum']+=value
        measurements.append(row)
    return dict(scope='DIAGNOSTIC: CPU FP64 analytical candidates versus unchanged OpenGL; not M3 acceptance',
                reference=str(directory),limits={k:v for k,v in metadata.items() if k!='cases'},
                colorGate=2e-5,pickingCandidate='write signed ID iff analytical visible coverage is positive',
                models={name:dict(maxColorError=max(r['models'][name]['maxColorError'] for r in measurements),
                                 idMismatches=sum(r['models'][name]['idMismatches'] for r in measurements),
                                 failingCases=sum(r['models'][name]['colorFailures']>0 or r['models'][name]['idMismatches']>0 for r in measurements))
                        for name in ('pixelArea','distanceRamp','majorAxisRamp')},cases=measurements)


def self_check():
    # Known box and diagonal strip intersections, independent of the driver.
    box=rectangle((2,3.5),(6,3.5),1)
    if abs(pixel_area(box,3,3)-1)>1e-12 or pixel_area(box,3,2)!=0:
        raise RuntimeError('Unit box coverage self-check failed')
    diagonal=rectangle((2,2),(6,6),1)
    expected=1-(1-1/math.sqrt(2))**2
    if abs(pixel_area(diagonal,3,3)-expected)>1e-12:
        raise RuntimeError('Diagonal strip coverage self-check failed')
    if abs(sum(pixel_area(diagonal,x,y) for y in range(8) for x in range(8))-math.sqrt(32))>1e-10:
        raise RuntimeError('Coverage conservation self-check failed')
    clipped=clip_segment((0,0,-2,1),(0,0,0,1),64)
    if clipped != ((32.0,32.0),(32.0,32.0)):
        raise RuntimeError('Homogeneous near clipping self-check failed')


def analyze_contract(directory, metadata_name='cases.json'):
    metadata=json.loads((directory/metadata_name).read_text())
    results=[]
    for case in metadata['cases']:
        extent=case['extent']
        shapes=[]
        for segment in case['segments']:
            clipped=clip_segment(segment['a'],segment['b'],extent,axes=(2,),depths=True)
            if clipped:
                a,b,z0,z1=clipped
                shapes.append((segment,a,b,z0,z1,rectangle(a,b,segment['width'])))
        data=(directory/case['file']).read_bytes()
        if len(data)!=extent*extent*24:
            raise RuntimeError('Invalid deterministic line readback length')
        max_color=max_depth=0.0
        ids=0
        for i,pixel in enumerate(struct.iter_unpack('<4fif',data)):
            if not all(math.isfinite(v) for v in (*pixel[:4],pixel[5])):
                raise RuntimeError('Nonfinite line readback')
            x,y=i%extent,i//extent
            color=[0.0]*4;entity=-1;depth=1.0
            clip=case['clip']
            inside=clip[2]<0 or (clip[0]<=x+.5<clip[0]+clip[2] and clip[1]<=y+.5<clip[1]+clip[3])
            if inside:
                for segment,a,b,z0,z1,polygon in shapes:
                    coverage=pixel_area(polygon,x,y)
                    alpha=max(0,min(1,segment['color'][3]))*coverage
                    if coverage<=1e-6 or alpha<=0:
                        continue
                    dx,dy=b[0]-a[0],b[1]-a[1]
                    length2=dx*dx+dy*dy
                    if length2==0:
                        continue
                    t=max(0,min(1,((x+.5-a[0])*dx+(y+.5-a[1])*dy)/length2))
                    z=z0+(z1-z0)*t
                    if case['depth'] and z>=depth:
                        continue
                    color=[segment['color'][c]*alpha+color[c]*(1-alpha) for c in range(3)]+[alpha+color[3]*(1-alpha)]
                    if segment['picking']:
                        entity=segment['id']
                    if case['depth']:
                        depth=z
            max_color=max(max_color,*(abs(c-v) for c,v in zip(color,pixel[:4])))
            max_depth=max(max_depth,abs(depth-pixel[5]))
            ids+=entity!=pixel[4]
        results.append(dict(name=case['name'],file=case['file'],pixels=extent*extent,
                            maxColorError=max_color,maxDepthError=max_depth,idMismatches=ids,
                            verdict='PASS' if max_color<=2e-5 and max_depth<=1e-6 and ids==0 else 'FAIL'))
    return dict(verdict='PASS' if all(r['verdict']=='PASS' for r in results) else 'FAIL',
                contract=metadata['contract'],cases=results)


def qualify(native,opengl):
    self_check()
    from macos.metal_m3 import compare
    if (native/'line-cases.json').read_bytes()!=(opengl/'cases.json').read_bytes():
        raise RuntimeError('Line workload metadata differs between backends')
    metadata=json.loads((opengl/'cases.json').read_text())
    parity={c['file']:compare(native/c['file'],opengl/c['file']) for c in metadata['cases']}
    with zipfile.ZipFile(Path(__file__).resolve().parents[2]/'tests/fixtures/renderer2d-lines-v1.zip') as archive:
        reference=zipfile.Path(archive)
        if (reference/'cases.json').read_bytes()!=(opengl/'cases.json').read_bytes():
            raise RuntimeError('Frozen line input cases changed; requires explicit reference review')
        references={backend:{c['file']:compare(directory/c['file'],reference/c['file']) for c in metadata['cases']}
                    for backend,directory in (('metal',native),('opengl',opengl))}
    result=dict(opengl=analyze_contract(opengl),metal=analyze_contract(native,'line-cases.json'),parity=parity,references=references)
    result['verdict']='PASS' if result['opengl']['verdict']==result['metal']['verdict']=='PASS' and all(r['verdict']=='PASS' for group in (parity,*references.values()) for r in group.values()) else 'FAIL'
    return result


def main():
    self_check()
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--legacy-reference',type=Path,help='Read preserved legacy captures; never renders or overwrites them')
    parser.add_argument('--captures',type=Path,help='Existing metal_m3.py output directory')
    args=parser.parse_args()
    if args.legacy_reference:
        result=analyze(args.legacy_reference)
        print(json.dumps(result['models'],indent=2))
    elif args.captures:
        result=qualify(args.captures/'metal',args.captures/'opengl/lines')
        (args.captures/'lines-results.json').write_text(json.dumps(result,indent=2)+'\n')
        print('Independent coverage/depth/IDs and cross-backend lines:',result['verdict'])
        if result['verdict']!='PASS':
            raise RuntimeError('Line qualification failed; see lines-results.json')
    else:
        parser.error('Select --legacy-reference or --captures')


if __name__=='__main__':
    main()
