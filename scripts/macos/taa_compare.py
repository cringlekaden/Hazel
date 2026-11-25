#!/usr/bin/env python3
"""Registered floor differences for the fixed TAA native AO motion fixture.

This measures residual changes, including physically changing occlusion. It is
not ground truth or owner visual acceptance. Pairs are four scene steps apart.
"""
import argparse
import json
import math
from pathlib import Path
import statistics
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from macos.xegtao import pixels

def compare(root):
    rows=[];width,height=320,180;projection_x=1/math.tan(math.pi/8)/(width/height)
    for quality in range(-1,4):
        images={}
        for index in range(16,64,4):
            for mode in ('off','on'):
                images[index,mode]=pixels(root/f'study-q{quality}-{index}-{mode}.bin')
        # Ground is the largest opaque surface in this maintained scene. Check
        # every registered bilinear tap has the same exact signed ID and depth.
        from collections import Counter
        floor=Counter(p[4] for p in images[16,'off'] if p[4]!=-1).most_common(1)[0][0]
        for mode in ('off','on'):
            differences=[];bias=[];pair_metrics=[]
            for index in range(20,64,4):
                previous=images[index-4,mode];current=images[index,mode]
                values=[]
                for y in range(3,height-3):
                    for x in range(3,width-3):
                        p=current[y*width+x]
                        if p[4]!=floor or p[5]>=1:continue
                        # GL perspective depth -> positive view distance, metres.
                        z=2*.1*100/(100+.1-(2*p[5]-1)*(100-.1))
                        old_x=x+.028*projection_x/z*.5*width
                        left=math.floor(old_x);fraction=old_x-left
                        if left<3 or left>=width-4:continue
                        valid=True
                        for yy in range(y-2,y+3):
                            for xx in range(x-2,x+3):
                                if current[yy*width+xx][4]!=floor:valid=False
                            for xx in range(left-2,left+4):
                                if previous[yy*width+xx][4]!=floor:valid=False
                        if not valid:continue
                        a,b=previous[y*width+left],previous[y*width+left+1]
                        if max(abs(a[5]-p[5]),abs(b[5]-p[5]))>2e-5:continue
                        delta=sum((p[c]-(a[c]*(1-fraction)+b[c]*fraction))*w for c,w in enumerate((.2126,.7152,.0722)))
                        values.append(delta);differences.append(delta*delta);bias.append(delta)
                pair_metrics.append(dict(frame=index,samples=len(values),rms=math.sqrt(statistics.mean(v*v for v in values)) if values else None))
            segments={}
            for name,predicate in (('cameraOnly',lambda frame:frame<=32),('movingOccluder',lambda frame:frame>=36)):
                pairs=[pair for pair in pair_metrics if predicate(pair['frame']) and pair['samples']]
                samples=sum(pair['samples'] for pair in pairs)
                segments[name]=dict(samples=samples,rms=math.sqrt(sum(pair['rms']**2*pair['samples'] for pair in pairs)/samples))
            rows.append(dict(quality=quality,taa=mode=='on',samples=len(differences),
                registeredFloorRMS=math.sqrt(statistics.mean(differences)),meanSignedChange=statistics.mean(bias),pairs=pair_metrics,segments=segments))
    comparisons=[]
    for quality in range(-1,4):
        off,on=rows[(quality+1)*2:(quality+1)*2+2]
        comparisons.append(dict(quality=quality,off=off['registeredFloorRMS'],on=on['registeredFloorRMS'],ratio=on['registeredFloorRMS']/off['registeredFloorRMS'],
            segmentRatios={name:on['segments'][name]['rms']/off['segments'][name]['rms'] for name in off['segments']}))
    return dict(scope=__doc__,status='MEASURED; owner acceptance pending',rows=rows,comparisons=comparisons)

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('captures',type=Path);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();report=compare(args.captures)
    args.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report['comparisons'],indent=2))
if __name__=='__main__':main()
