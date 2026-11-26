#!/usr/bin/env python3
"""Bounded, isolated PCSS shader optimization candidates; no production selection."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import shutil
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from macos.metal_shaders import produce

VECTOR='''float softComparison(int cascade,vec3 p,vec2 slope,vec2 delta,float scale,float bias){
    int r=int(u_ShadowParameters.x);ivec2 origin=ivec2(cascade%2,cascade/2)*r;
    float tx=u_ShadowTexelDepth[cascade].x,inv=u_ShadowTexelDepth[cascade].y;
    vec2 coordinate=p.xy*float(r)-.5+delta/tx;ivec2 cell=ivec2(floor(coordinate));vec2 f=fract(coordinate);
    ivec2 low=clamp(cell,ivec2(0),ivec2(r-1)),high=clamp(cell+1,ivec2(0),ivec2(r-1));
    vec4 depths;
    FETCH
    vec4 dx=(vec4(low.x,high.x,low.x,high.x)+.5-p.x*float(r))*tx;
    vec4 dy=(vec4(low.y,low.y,high.y,high.y)+.5-p.y*float(r))*tx;
    vec4 receiver=p.z+(slope.x*dx+slope.y*dy)*inv-bias*u_ShadowParameters.z*scale*inv;
    vec4 weights=vec4((1-f.x)*(1-f.y),f.x*(1-f.y),(1-f.x)*f.y,f.x*f.y);
    return dot(max(vec4(lessThanEqual(receiver,depths)),vec4(greaterThanEqual(depths,vec4(1)))),weights);
}
'''
SCALAR='''depths=vec4(texelFetch(u_ShadowAtlas,origin+low,0).r,
        texelFetch(u_ShadowAtlas,origin+ivec2(high.x,low.y),0).r,
        texelFetch(u_ShadowAtlas,origin+ivec2(low.x,high.y),0).r,
        texelFetch(u_ShadowAtlas,origin+high,0).r);'''
GATHER='''if(any(lessThan(cell,ivec2(0)))||any(greaterThanEqual(cell+1,ivec2(r)))){SCALAR}
    else depths=textureGather(u_ShadowAtlas,(vec2(origin+cell)+1)/float(2*r),0).wzxy;'''.replace('SCALAR',SCALAR)


def prepare(kind,base,destination,shade_only=False,early_shadow=False,filter_count=16,search_count=16,explicit_lod=False,vogel_filter=False,unroll_filter=False,fused_filter=False,early_geometry=False,small_contact=False,refine_empty=False):
    engine=Path(__file__).resolve().parents[2]/'Hazel/Resources/shaders/ForwardPBR.glsl'
    if 'softShadow(' not in engine.read_text():
        raise RuntimeError('Generating PCSS candidates requires the recorded prototype source patch and matching rebuilt MetalShaderTools; production remains PCF.')
    shutil.copytree(base,destination)
    file=destination/'shaders/ForwardPBR.glsl';source=file.read_text();original=source
    start=source.index('float softComparison(');end=source.index('// Returns visibility',start)
    body=VECTOR.replace('FETCH',GATHER if kind=='gather-vector' else SCALAR)
    if kind.startswith('hardware'):
        source=source.replace('layout(binding=10) uniform sampler2D u_ShadowAtlas;','layout(binding=10) uniform sampler2D u_ShadowAtlas;\nlayout(binding=11) uniform sampler2DShadow u_ShadowCompareAtlas;')
        fast='''
    // Hardware bilinear compares one reference for all four depths. Use it only
    // where the geometric receiver plane is flat in light space; retain exact
    // per-texel correction on sloped receivers, without extra slope bias.
    if(all(lessThanEqual(abs(slope),vec2(1e-6)))){
        vec2 uv=(vec2(origin)+p.xy*float(r)+delta/tx)/float(2*r);
        uv=clamp(uv,(vec2(origin)+.5)/float(2*r),(vec2(origin+r)-.5)/float(2*r));
        float receiver=p.z-bias*u_ShadowParameters.z*scale*inv;
        return texture(u_ShadowCompareAtlas,vec3(uv,clamp(receiver,0,1)));
    }
'''
        if kind=='hardware-plane':
            fast=fast.replace('if(all(lessThanEqual(abs(slope),vec2(1e-6)))){','{')
            fast=fast.replace('float receiver=p.z-bias*u_ShadowParameters.z*scale*inv;','''vec2 extent=max(f,1-f)*tx;
        // Conservative native bilinear-footprint envelope, not a penumbra-sized
        // slope bias. This has an explicit contact-placement tradeoff to qualify.
        float planeEnvelope=dot(abs(slope),extent);
        float receiver=p.z+dot(slope,delta)*inv-(bias*u_ShadowParameters.z*scale+planeEnvelope)*inv;''')
        body=body.replace('    vec4 depths;',fast+'    vec4 depths;')
        if kind=='hardware-exact':
            # Compare at exact texel centres with each texel's own receiver depth.
            # The same linear comparison sampler has zero interpolation fraction
            # there. Preserve geometric correction without a slope-bias envelope.
            body=body.replace('    vec4 depths;\n    '+SCALAR,'')
            old='return dot(max(vec4(lessThanEqual(receiver,depths)),vec4(greaterThanEqual(depths,vec4(1)))),weights);'
            body=body.replace(old,'''vec2 factor=vec2(1.0/float(2*r));receiver=clamp(receiver,0,1);
    vec4 visibility=vec4(
        texture(u_ShadowCompareAtlas,vec3((vec2(origin+low)+.5)*factor,receiver.x)),
        texture(u_ShadowCompareAtlas,vec3((vec2(origin+ivec2(high.x,low.y))+.5)*factor,receiver.y)),
        texture(u_ShadowCompareAtlas,vec3((vec2(origin+ivec2(low.x,high.y))+.5)*factor,receiver.z)),
        texture(u_ShadowCompareAtlas,vec3((vec2(origin+high)+.5)*factor,receiver.w)));
    return dot(visibility,weights);''')
    start=source.index('float softComparison(');end=source.index('// Returns visibility',start)
    source=source[:start]+body+source[end:]
    if explicit_lod and kind.startswith('hardware'):
        # Explicit LOD is valid in nonuniform PCSS control flow; this atlas has
        # one level. Keep it separate to measure actual generated sampling ops.
        source=source.replace('return texture(u_ShadowCompareAtlas,vec3(uv,clamp(receiver,0,1)));','return textureLod(u_ShadowCompareAtlas,vec3(uv,clamp(receiver,0,1)),0);')
        for suffix,component in [('low','x'),('ivec2(high.x,low.y)','y'),('ivec2(low.x,high.y)','z'),('high','w')]:
            source=source.replace('texture(u_ShadowCompareAtlas,vec3((vec2(origin+'+suffix+')+.5)*factor,receiver.'+component+'))','textureLod(u_ShadowCompareAtlas,vec3((vec2(origin+'+suffix+')+.5)*factor,receiver.'+component+'),0)')
    if kind=='unrolled-vector':
        old='for(int i=0;i<16;++i)visibility+=softComparison(cascade,p,slope,shadowDisk[i]*radius,scale,bias)/16;'
        assert old in source
        source=source.replace(old,'\n    '.join('visibility+=softComparison(cascade,p,slope,shadowDisk['+str(i)+']*radius,scale,bias)/16;' for i in range(16)))
    if search_count!=16:
        points=[(0.,0.)]+[(math.sqrt((i+.5)/(search_count-1))*math.cos(i*math.pi*(3-math.sqrt(5))),math.sqrt((i+.5)/(search_count-1))*math.sin(i*math.pi*(3-math.sqrt(5)))) for i in range(search_count-1)]
        declaration='const vec2 shadowSearchDisk['+str(search_count)+']=vec2[]('+','.join('vec2('+format(x,'.9f')+','+format(y,'.9f')+')' for x,y in points)+');\n'
        source=source.replace('float softReceiverScale(',declaration+'float softReceiverScale(',1)
        source=source.replace('for(int i=0;i<16;++i){\n        // One centre sample','for(int i=0;i<'+str(search_count)+';++i){\n        // One centre sample')
        source=source.replace('(i==5?vec2(0):shadowDisk[i])*search','shadowSearchDisk[i]*search').replace('count/16','count/'+str(search_count))
    if filter_count!=16 or vogel_filter:
        points=[(math.sqrt((i+.5)/filter_count)*math.cos(i*math.pi*(3-math.sqrt(5))),math.sqrt((i+.5)/filter_count)*math.sin(i*math.pi*(3-math.sqrt(5)))) for i in range(filter_count)]
        declaration='const vec2 shadowFilterDisk['+str(filter_count)+']=vec2[]('+','.join('vec2('+format(x,'.9f')+','+format(y,'.9f')+')' for x,y in points)+');\n'
        source=source.replace('float softReceiverScale(',declaration+'float softReceiverScale(',1)
        needle='for(int i=0;i<16;++i)visibility+=softComparison(cascade,p,slope,shadowDisk[i]*radius,scale,bias)/16;'
        assert needle in source
        source=source.replace(needle,'for(int i=0;i<'+str(filter_count)+';++i)visibility+=softComparison(cascade,p,slope,shadowFilterDisk[i]*radius,scale,bias)/'+str(filter_count)+';')
    if unroll_filter:
        disk='shadowFilterDisk' if filter_count!=16 or vogel_filter else 'shadowDisk'
        needle='for(int i=0;i<'+str(filter_count)+';++i)visibility+=softComparison(cascade,p,slope,'+disk+'[i]*radius,scale,bias)/'+str(filter_count)+';'
        assert needle in source
        source=source.replace(needle,'\n    '.join('visibility+=softComparison(cascade,p,slope,'+disk+'['+str(i)+']*radius,scale,bias)/'+str(filter_count)+';' for i in range(filter_count)))
    if fused_filter:
        assert kind=='hardware-plane' and not unroll_filter
        disk='shadowFilterDisk' if filter_count!=16 or vogel_filter else 'shadowDisk'
        points=points if filter_count!=16 or vogel_filter else []
        assert points
        mx=sum(x for x,y in points)/filter_count;my=sum(y for x,y in points)/filter_count;mxy=sum(x*y for x,y in points)/filter_count
        exact=VECTOR.replace('softComparison(','softExactComparison(').replace('FETCH',SCALAR)
        contact=exact.replace('softExactComparison(','softContactComparison(').replace('vec2 delta,float scale','vec2 delta,float scale')
        # delta.x carries radius / texel; all locations use the same2x2 cells.
        contact=contact.replace('p.xy*float(r)-.5+delta/tx','p.xy*float(r)-.5')
        contact=contact.replace('vec4 weights=vec4((1-f.x)*(1-f.y),f.x*(1-f.y),(1-f.x)*f.y,f.x*f.y);',f'''float ax={mx:.12f}*delta.x,ay={my:.12f}*delta.x,axy={mxy:.12f}*delta.x*delta.x;
    vec4 weights=vec4((1-f.x)*(1-f.y)-(1-f.y)*ax-(1-f.x)*ay+axy,
        f.x*(1-f.y)+(1-f.y)*ax-f.x*ay-axy,
        (1-f.x)*f.y-f.y*ax+(1-f.x)*ay-axy,
        f.x*f.y+f.y*ax+f.x*ay+axy);''')
        fused='''float softFusedFilter(int cascade,vec3 p,vec2 slope,float radius,float scale,float bias,float nearest){
    float tx=u_ShadowTexelDepth[cascade].x,inv=u_ShadowTexelDepth[cascade].y;
    float planeEnvelope=dot(abs(slope),vec2(tx));
    vec2 f=fract(p.xy*u_ShadowParameters.x-.5);
    if(nearest<=4*planeEnvelope){
        if(radius==0)return softExactComparison(cascade,p,slope,vec2(0),scale,bias);
        if(all(greaterThan(min(f,1-f),vec2(radius/tx))))return softContactComparison(cascade,p,slope,vec2(radius/tx,0),scale,bias);
        float sum=0;
        for(int i=0;i<FILTER_COUNT;++i)sum+=softExactComparison(cascade,p,slope,DISK[i]*radius,scale,bias)/FILTER_COUNT;
        return sum;
    }
    int r=int(u_ShadowParameters.x);ivec2 origin=ivec2(cascade%2,cascade/2)*r;
    vec2 uv=(vec2(origin)+p.xy*float(r))/float(2*r),uvRadius=vec2(radius/(2*tx*float(r)));
    vec2 margin=vec2((radius+tx)/(tx*float(r)));
    if(any(lessThan(p.xy,margin))||any(greaterThan(p.xy,1-margin))){
        float sum=0;
        for(int i=0;i<FILTER_COUNT;++i)sum+=softComparison(cascade,p,slope,DISK[i]*radius,scale,bias)/FILTER_COUNT;
        return sum;
    }
    float receiver=p.z-(bias*u_ShadowParameters.z*scale+planeEnvelope)*inv;
    vec2 receiverRadius=slope*(radius*inv);float sum=0;
    for(int i=0;i<FILTER_COUNT;++i){
        vec2 sampleOffset=DISK[i];
        sum+=textureLod(u_ShadowCompareAtlas,vec3(uv+sampleOffset*uvRadius,clamp(receiver+dot(receiverRadius,sampleOffset),0,1)),0)/FILTER_COUNT;
    }
    return sum;
}
'''.replace('FILTER_COUNT',str(filter_count)).replace('DISK',disk)
        small=''
        if small_contact:
            # A disk with diameter <=one texel touches at most3x3 native depths.
            # Integrate the original finite tent kernel before loading depths;
            # retain each actual texel's geometric receiver-plane comparison.
            small='''float softSmallContact(int cascade,vec3 p,vec2 slope,float radius,float scale,float bias){
    int r=int(u_ShadowParameters.x);ivec2 origin=ivec2(cascade%2,cascade/2)*r;
    float tx=u_ShadowTexelDepth[cascade].x,inv=u_ShadowTexelDepth[cascade].y;
    vec2 coordinate=p.xy*float(r)-.5;ivec2 cell=ivec2(floor(coordinate-.5));
    vec2 f=coordinate-vec2(cell);vec3 w0=vec3(0),w1=vec3(0),w2=vec3(0);
    for(int i=0;i<FILTER_COUNT;++i){
        vec2 q=f+DISK[i]*(radius/tx);
        vec3 x=max(1-abs(vec3(0,1,2)-q.x),0)/FILTER_COUNT;
        vec3 y=max(1-abs(vec3(0,1,2)-q.y),0);
        w0+=x*y.x;w1+=x*y.y;w2+=x*y.z;
    }
    float sum=0;
    for(int y=0;y<3;++y){
        vec3 w=y==0?w0:y==1?w1:w2;
        for(int x=0;x<3;++x){
            ivec2 tap=clamp(cell+ivec2(x,y),ivec2(0),ivec2(r-1));
            float stored=texelFetch(u_ShadowAtlas,origin+tap,0).r;
            vec2 delta=(vec2(tap)+.5-p.xy*float(r))*tx;
            float receiver=p.z+(dot(slope,delta)-bias*u_ShadowParameters.z*scale)*inv;
            sum+=w[x]*float(stored>=1||receiver<=stored);
        }
    }
    return sum;
}
'''.replace('FILTER_COUNT',str(filter_count)).replace('DISK',disk)
            fused=fused.replace('        float sum=0;\n        for(int i=0;', '        if(radius<=.5*tx)return softSmallContact(cascade,p,slope,radius,scale,bias);\n        float sum=0;\n        for(int i=0;',1)
        at=source.index('// Returns visibility');source=source[:at]+exact+contact+small+fused+source[at:]
        source=source.replace('softComparison(cascade,p,slope,vec2(0),scale,bias),0,0,0','softExactComparison(cascade,p,slope,vec2(0),scale,bias),0,0,0')
        source=source.replace('float separation=0,count=0;','float separation=0,count=0,nearest=1e30;')
        needle='separation+=max((actual-stored)/inverseDepth,0);count+=1;'
        assert needle in source;source=source.replace(needle,'float physical=max((actual-stored)/inverseDepth,0);separation+=physical;nearest=min(nearest,physical);count+=1;')
        begin=source.index('    float visibility=0;',source.index('    float radius=min(separation*'))
        end=source.index('    return vec4(visibility,radius,separation,count/',begin)
        source=source[:begin]+'    float visibility=softFusedFilter(cascade,p,slope,radius,scale,bias,nearest);\n'+source[end:]
    if refine_empty:
        assert fused_filter
        source=source.replace('    if(count==0)return vec4(1,0,0,0);','''    // Point search can miss the finite bilinear coverage of an otherwise resolved
    // edge, especially as the physical radius tends to zero. Inspect its exact
    // native footprint before accepting an empty search; do not invent blockers.
    if(count==0){
        vec2 coordinate=p.xy*float(resolution)-.5;
        ivec2 cell=ivec2(floor(coordinate));vec2 f=fract(coordinate);
        for(int y=0;y<2;++y)for(int x=0;x<2;++x){
            float weight=(x==0?1-f.x:f.x)*(y==0?1-f.y:f.y);
            ivec2 tap=clamp(cell+ivec2(x,y),ivec2(0),ivec2(resolution-1));
            float stored=texelFetch(u_ShadowAtlas,origin+tap,0).r;
            vec2 delta=(vec2(tap)+.5-p.xy*float(resolution))*texel;
            float receiver=p.z+dot(slope,delta)*inverseDepth;
            if(weight>0&&stored<1&&stored<receiver-bias*u_ShadowParameters.z*scale*inverseDepth){
                vec2 actualDelta=(vec2(tap)+.5-base.xy*float(resolution))*texel;
                float physical=max((base.z+dot(slope,actualDelta)*inverseDepth-stored)/inverseDepth,0);
                separation+=physical*weight;count+=weight;nearest=min(nearest,physical);
            }
        }
        if(count==0)return vec4(1,0,0,0);
    }''')
    if shade_only:
        source=source.replace('    if(u_ShadowSoftness.w>.5)return softShadow(cascade,position,Ng,L).x;','    return softShadow(cascade,position,Ng,L).x;')
        source=source.replace('    if(u_ShadowRange.w>0)result=shadowInspection(v_Position,geometricNormal,normalize(u_Light.xyz));','')
        source=source.replace('bias=u_ShadowRange.w==10?0:1','bias=1')
    if early_shadow:
        old='''        SurfaceEnergy directEnergy=surfaceEnergy(f0,sampleBRDF(clamp(dot(directNormal,V),0,1),r));
        vec3 L=normalize(u_Light.xyz);
        if(dot(surfaceNormal,V)>0&&dot(surfaceNormal,L)>0)
            directTerm=direct(base.rgb,metal,rough,directNormal,V,L,directEnergy)*(u_Incident.rgb*directionalVisibility(v_Position,geometricNormal,L))*((maps&4)!=0?surfaceVisibility(surfaceNormal,directNormal,L):1);'''
        new='''        vec3 L=normalize(u_Light.xyz);
        bool directionalGate=dot(surfaceNormal,V)>0&&dot(surfaceNormal,L)>0;
        float keyVisibility=directionalGate?directionalVisibility(v_Position,geometricNormal,L):0;
        // Keep BRDF intermediates out of the shadow loop's live range. Visibility
        // zero skips only directional direct BRDF work, preserving locals/IBL.
        SurfaceEnergy directEnergy;
        if(keyVisibility>0||u_LocalMeta.x>0)directEnergy=surfaceEnergy(f0,sampleBRDF(clamp(dot(directNormal,V),0,1),r));
        if(directionalGate&&keyVisibility>0)
            directTerm=direct(base.rgb,metal,rough,directNormal,V,L,directEnergy)*(u_Incident.rgb*keyVisibility)*((maps&4)!=0?surfaceVisibility(surfaceNormal,directNormal,L):1);'''
        assert old in source;source=source.replace(old,new)
    if early_geometry:
        assert early_shadow
        old='''        vec3 L=normalize(u_Light.xyz);
        bool directionalGate=dot(surfaceNormal,V)>0&&dot(surfaceNormal,L)>0;
        float keyVisibility=directionalGate?directionalVisibility(v_Position,geometricNormal,L):0;
'''
        assert old in source;source=source.replace(old,'')
        at='    vec3 T=normalize(v_Tangent.xyz-N*dot(v_Tangent.xyz,N));'
        assert at in source
        source=source.replace(at,'''    vec3 L=normalize(u_Light.xyz);
    vec3 shadowSurface=baseNormal;
    if(u_Options.w>.5&&!gl_FrontFacing)shadowSurface=-shadowSurface;
    bool directionalGate=u_Options.x<.5&&dot(shadowSurface,V)>0&&dot(shadowSurface,L)>0;
    float keyVisibility=directionalGate?directionalVisibility(v_Position,geometricNormal,L):0;
'''+at)
    file.write_text(source);produce('Release',destination/'shaders/metal',sources=destination/'shaders')
    result=dict(kind=kind,shadeOnly=shade_only,earlyShadow=early_shadow,earlyGeometry=early_geometry,explicitLod=explicit_lod,unrollFilter=unroll_filter,fusedFilter=fused_filter,smallContact=small_contact,refineEmpty=refine_empty,filterPattern='vogel' if filter_count!=16 or vogel_filter else 'concentric',baseShaderSHA256=hashlib.sha256(original.encode()).hexdigest(),sourceSHA256=hashlib.sha256(source.encode()).hexdigest(),blockerLocations=search_count,filterLocations=filter_count,comparison='native plane envelope' if kind=='hardware-plane' else 'exact per-texel geometric receiver plane; numerical order may differ',productionSelected=False)
    (destination/'candidate.json').write_text(json.dumps(result,indent=2)+'\n')


def prepare_exact_filter(base,destination,gather):
    """Fuse the wide exact fallback, preserving each native receiver comparison."""
    engine=Path(__file__).resolve().parents[2]/'Hazel/Resources/shaders/ForwardPBR.glsl'
    if 'softShadow(' not in engine.read_text():
        raise RuntimeError('Generating PCSS candidates requires the recorded prototype source patch and matching rebuilt MetalShaderTools; production remains PCF.')
    shutil.copytree(base,destination)
    file=destination/'shaders/ForwardPBR.glsl';source=file.read_text();original=source
    fetch='''vec4 depths=vec4(texelFetch(u_ShadowAtlas,origin+cell,0).r,
            texelFetch(u_ShadowAtlas,origin+cell+ivec2(1,0),0).r,
            texelFetch(u_ShadowAtlas,origin+cell+ivec2(0,1),0).r,
            texelFetch(u_ShadowAtlas,origin+cell+ivec2(1),0).r);'''
    if gather:fetch='vec4 depths=textureGather(u_ShadowAtlas,(vec2(origin+cell)+1)/float(2*r),0).wzxy;'
    helper='''float softExactFilter(int cascade,vec3 p,vec2 slope,float radius,float scale,float bias){
    int r=int(u_ShadowParameters.x);ivec2 origin=ivec2(cascade%2,cascade/2)*r;
    float tx=u_ShadowTexelDepth[cascade].x,inv=u_ShadowTexelDepth[cascade].y;
    vec2 margin=vec2((radius+tx)/(tx*float(r)));
    if(any(lessThan(p.xy,margin))||any(greaterThan(p.xy,1-margin))){
        float sum=0;for(int i=0;i<32;++i)sum+=softExactComparison(cascade,p,slope,shadowFilterDisk[i]*radius,scale,bias)/32;
        return sum;
    }
    vec2 centre=p.xy*float(r),coordinateBase=centre-.5;
    float comparisonBias=bias*u_ShadowParameters.z*scale*inv;float sum=0;
    for(int i=0;i<32;++i){
        vec2 coordinate=coordinateBase+(shadowFilterDisk[i]*radius)/tx;
        ivec2 cell=ivec2(floor(coordinate));vec2 f=fract(coordinate);
        FETCH
        vec4 dx=(vec4(cell.x,cell.x+1,cell.x,cell.x+1)+.5-centre.x)*tx;
        vec4 dy=(vec4(cell.y,cell.y,cell.y+1,cell.y+1)+.5-centre.y)*tx;
        vec4 receiver=p.z+(slope.x*dx+slope.y*dy)*inv-comparisonBias;
        vec4 weights=vec4((1-f.x)*(1-f.y),f.x*(1-f.y),(1-f.x)*f.y,f.x*f.y);
        sum+=dot(max(vec4(lessThanEqual(receiver,depths)),vec4(greaterThanEqual(depths,vec4(1)))),weights)/32;
    }
    return sum;
}
'''.replace('FETCH',fetch)
    at=source.index('float softFusedFilter(');source=source[:at]+helper+source[at:]
    needle='''        float sum=0;
        for(int i=0;i<32;++i)sum+=softExactComparison(cascade,p,slope,shadowFilterDisk[i]*radius,scale,bias)/32;
        return sum;'''
    assert needle in source;source=source.replace(needle,'        return softExactFilter(cascade,p,slope,radius,scale,bias);',1)
    file.write_text(source);produce('Release',destination/'shaders/metal',sources=destination/'shaders')
    metadata=dict(kind='fused-exact-gather' if gather else 'fused-exact-scalar',blockerLocations=8,filterLocations=32,filterPattern='vogel',comparison='native plane envelope',fusedFilter=True,smallContact=True,refineEmpty=True,receiverPlaneBounds=True,baseShaderSHA256=hashlib.sha256(original.encode()).hexdigest(),sourceSHA256=hashlib.sha256(source.encode()).hexdigest(),productionSelected=False)
    (destination/'candidate.json').write_text(json.dumps(metadata,indent=2)+'\n')


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('kind',choices=('vector','gather-vector','unrolled-vector','hardware-flat','hardware-plane','hardware-exact'));p.add_argument('--base',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--shade-only',action='store_true');p.add_argument('--early-shadow',action='store_true');p.add_argument('--filter-count',type=int,choices=(16,24,32),default=16);p.add_argument('--search-count',type=int,choices=(8,12,16),default=16);p.add_argument('--explicit-lod',action='store_true');p.add_argument('--vogel-filter',action='store_true');a=p.parse_args();prepare(a.kind,a.base.resolve(),a.output.resolve(),a.shade_only,a.early_shadow,a.filter_count,a.search_count,a.explicit_lod,a.vogel_filter)


if __name__=='__main__':main()
