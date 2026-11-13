#!/usr/bin/env python3
"""Bounded, offline A/B SH screening. Never writes shipping resources.
Requires numpy==2.3.5 and PyYAML==6.0.3 in an isolated environment.
See docs/design/ibl-ab-compression-experiment.md for the pre-fit contract.
"""
import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import resource
import signal
import shutil
import subprocess
import time

# Set BEFORE importing numpy: one bounded heavy job, one numerical worker.
for key in ('OPENBLAS_NUM_THREADS', 'OMP_NUM_THREADS', 'VECLIB_MAXIMUM_THREADS'):
    os.environ[key] = '1'
import numpy as np
import yaml

ROOT = Path(__file__).resolve().parents[2]
NV = np.array([.0005,.002,.01,.02,.05,.1,.2,.35,.5,.7,.85,1.])
ROUGH = np.array([.045,.05,.1,.2,.35,.45,.6,.75,1.])
RANKS = (8,16,32)
MODES = [(l,m) for l in range(8) for m in range(l+1)]
FULL = [(l,m) for l in range(8) for m in range(-l,l+1)]
SOURCES = dict(studio='examples/RenderingShowcase/Assets/Environments/Studio.hdr',
 axes='examples/RenderingReference/Assets/Reference/IBL_v1/Sources/AsymmetricAxes.hdr',
 tiny='examples/RenderingReference/Assets/Reference/IBLQuality_v2/TinyEmitter.hdr')


def write_json(path, obj):
    with path.open('x') as f:
        json.dump(obj, f, indent=2, allow_nan=False)


def norm(x):
    return x / np.linalg.norm(x, axis=-1, keepdims=True)


def extract(capture):
    """UUID-based extraction, never historical native entity handles."""
    meta = yaml.safe_load((capture/'capture.yaml').read_text())
    w,h = meta['Width'],meta['Height']
    assert (w,h)==(1280,720) and meta['Exposure']==0 and meta['Gain']==1
    yaw=meta['Yaw']; c,s=math.cos(yaw),math.sin(yaw)
    rotation=np.array([[c,0,-s],[0,1,0],[s,0,c]])
    all_rows, metadata = [],[]
    for step in (-6,-3,0,3,6):
        def raw(k, ids=False):
            return np.memmap(capture/f'dark-{step}-{k}.{"ids32" if ids else "rgba32f"}',
                             mode='r', dtype='<i4' if ids else '<f4').reshape((-1,) if ids else (-1,4))
        ids=raw(8,True); f={k:raw(k) for k in (1,2,4,8,11,12,13,15)}
        eye=np.array(meta['Views'][step]['Eye'])
        groups=[('sharp',110),('r20',111),('r45',112),('r75',113),('copper',114),('wood',121)]
        for group,uuid in groups:
            if step!=0 and uuid not in (114,121): continue
            handle=meta['Entities'].get(str(uuid),meta['Entities'].get(uuid))
            good=(ids==handle)&(f[15][:,0]>.02)&(f[15][:,1]>.001)
            good[:w]=False; good[-w:]=False
            good[np.arange(w*h)%w==0]=False;good[np.arange(w*h)%w==w-1]=False
            for offset in (-w,w,-1,1): good &= np.roll(ids,-offset)==handle
            indices=np.flatnonzero(good)
            def add(i,label):
                N=norm(2*f[2][i,:3].astype(float)-1)@rotation.T
                V=norm(eye-f[12][i,:3])@rotation.T
                Ng=norm(f[13][i,:3].astype(float))@rotation.T
                row=np.r_[N,V,Ng,f[1][i,:3],f[11][i,1:3],f[4][i,2]]
                all_rows.append(row);metadata.append(dict(group=label,step=step,uuid=uuid,
                  pixel=[int(i%w),int(h-1-i//w)],actual=f[8][i,:3].astype(float).tolist()))
            if step==0:
                for q in range(24): add(indices[int((q+.5)*len(indices)/24)],'actual/'+group)
            if uuid in (114,121):
                for q in range(8): add(indices[int((q+.5)*len(indices)/8)],'orbit/'+group)
        if step==0:
            for x,y in ((902,567),(893,525),(916,586),(612,200),(619,194),(585,267)):
                # add() uses only pixel data; UUID is metadata for this named row.
                i=(h-1-y)*w+x
                uuid=next(int(k) for k,v in meta['Entities'].items() if v==ids[i])
                add(i,'actual/named')
    sweep=np.loadtxt(capture/'sweep-inputs.txt')
    for row in sweep:
        all_rows.append(row);metadata.append(dict(group='sweep',step=None))
    # Stored and midpoint view/roughness pairs, independent directions including
    # longitude seam, cube-edge directions and two different tangent rolls.
    for kind,vs,rs in [('node',[.02,.2,1.],[.045,.2,.45,1.]),
                       ('midpoint',[.015,.275,.775],[.0475,.275,.525,.875])]:
        for orientation in (0,1):
            N=norm(np.array([-1.,.2,1e-5]) if orientation==0 else np.array([1.,1.,1.]))
            X=norm(np.cross([0.,1.,0.],N));Y=np.cross(N,X)
            X=math.cos(.37)*X+math.sin(.37)*Y if orientation else X
            for nv in vs:
                V=nv*N+math.sqrt(1-nv*nv)*X
                for r in rs:
                    for metal in (0.,.5,1.):
                        all_rows.append(np.r_[N,V,N,[.18,.4,.7],r,metal,.8])
                        metadata.append(dict(group=kind,orientation=orientation))
    rows=np.array(all_rows)
    assert len(rows)<=1024
    rows[:,:3]=norm(rows[:,:3]); rows[:,3:6]=norm(rows[:,3:6]);rows[:,6:9]=norm(rows[:,6:9])
    return rows,metadata,meta


def hdr(path):
    """Exact RGBE decoding of bounded tracked -Y/+X fixtures, no resampling."""
    assert path.stat().st_size<=64*1024**2
    with path.open('rb') as f:
        assert f.readline().startswith(b'#?')
        while f.readline().strip(): pass
        orient,h,xorient,w=f.readline().decode().split()
        h,w=int(h),int(w)
        assert orient=='-Y' and xorient=='+X' and w*h<=2048*1024
        pixels=np.empty((h,w,4),dtype=np.uint8)
        for y in range(h):
            header=f.read(4)
            if header[:2]!=b'\x02\x02':
                scan=header+f.read(4*(w-1))
                assert len(scan)==4*w
                pixels[y]=np.frombuffer(scan,dtype=np.uint8).reshape(w,4)
                # The tracked flat fixtures do not contain old-style repeat
                # markers. Reject that encoding instead of silently misreading.
                assert not np.any(np.all(pixels[y,:,:3]==1,axis=1))
                continue
            assert int.from_bytes(header[2:],'big')==w
            for c in range(4):
                i=0
                while i<w:
                    count=f.read(1)[0]
                    if count>128:
                        n=count-128;pixels[y,i:i+n,c]=f.read(1)[0]
                    else:
                        n=count;pixels[y,i:i+n,c]=np.frombuffer(f.read(n),dtype=np.uint8)
                    assert n>0 and i+n<=w
                    i+=n
    return pixels[:,:,:3].astype(float)*np.exp2(pixels[:,:,3].astype(float)-136)[:,:,None]


def sh(directions, modes):
    """Orthonormal real SH, Condon-Shortley, in a Z-polar frame."""
    z=np.clip(directions[:,2],-1,1); phi=np.arctan2(directions[:,1],directions[:,0])
    maxl=max(l for l,m in modes); P={};pmm=np.ones(len(z));radius=np.sqrt(np.maximum(0,1-z*z))
    for m in range(maxl+1):
        if m: pmm=-(2*m-1)*radius*pmm
        P[m,m]=pmm
        if m<maxl:P[m+1,m]=(2*m+1)*z*pmm
        for l in range(m+2,maxl+1):P[l,m]=((2*l-1)*z*P[l-1,m]-(l+m-1)*P[l-2,m])/(l-m)
    out=np.empty((len(z),len(modes)))
    for i,(l,m) in enumerate(modes):
        a=abs(m);n=math.sqrt((2*l+1)/(4*math.pi)*math.factorial(l-a)/math.factorial(l+a))
        out[:,i]=n*P[l,a]*(1 if m==0 else math.sqrt(2)*(np.cos(a*phi) if m>0 else np.sin(a*phi)))
    return out


def project_kernel(nv,rough,order):
    """Candidate estimator: NDF-CDF Gauss/midpoint angular projection."""
    u,w=np.polynomial.legendre.leggauss(order);u=(u+1)/2;w=w/2
    phi=2*np.pi*(np.arange(4*order)+.5)/(4*order)
    a2=max(.045,rough)**4
    nh=np.sqrt((1-u)/(1+(a2-1)*u));radius=np.sqrt(1-nh*nh)
    out=np.zeros((2,len(MODES)))
    for start in range(0,order,max(1,65536//(4*order))):
        sl=slice(start,min(order,start+max(1,65536//(4*order))))
        H=np.stack(np.broadcast_arrays(radius[sl,None]*np.cos(phi),radius[sl,None]*np.sin(phi),nh[sl,None]),axis=-1).reshape(-1,3)
        vh=H[:,0]*math.sqrt(1-nv*nv)+H[:,2]*nv
        L=2*vh[:,None]*H-np.array([math.sqrt(1-nv*nv),0,nv]);nl=L[:,2]
        good=(vh>0)&(nl>0)
        vis=.5/(np.maximum(nl,0)*math.sqrt(nv*nv*(1-a2)+a2)+nv*np.sqrt(nl*nl*(1-a2)+a2))
        weight=4*nl*vis*vh/H[:,2]*np.repeat(w[sl],4*order)/(4*order);weight[~good]=0
        fc=(1-np.clip(vh,0,1))**5
        basis=sh(norm(L),MODES)
        out[0]+=((1-fc)*weight)@basis;out[1]+=(fc*weight)@basis
    return out


def source_coefficients(image,order):
    """FP64 Gauss projection split at bilinear knots, including clamped poles.

    This projects lighting only; no BRDF, fitted coefficients or oracle is used.
    Orders 3/6 establish projection uncertainty without a source-dependent fit.
    """
    h,w,_=image.shape; coeff=np.zeros((64,3))
    q,weights=np.polynomial.legendre.leggauss(order);q=(q+1)/2;weights=weights/2
    # Any full longitude period is equivalent. Starting at pixel-center x=0
    # makes each cell bilinear with the final cell wrapping to pixel-center 0.
    x=(np.arange(w)[:,None]+q).reshape(-1);tx=np.tile(q,w);ix=np.repeat(np.arange(w),order)
    phi=2*np.pi*((x+.5)/w-.5);wx=np.tile(weights,w)*2*np.pi/w
    bounds=np.r_[-.5,np.arange(h),h-.5]
    for ya,yb in zip(bounds[:-1],bounds[1:]):
        for qy,wy in zip(q,weights):
            y=ya+(yb-ya)*qy;iy=math.floor(y);ty=y-iy
            def at(yy,xx):return image[np.clip(yy,0,h-1),xx%w]
            E=(1-ty)*((1-tx[:,None])*at(iy,ix)+tx[:,None]*at(iy,ix+1))+ty*((1-tx[:,None])*at(iy+1,ix)+tx[:,None]*at(iy+1,ix+1))
            theta=np.pi*(y+.5)/h
            d=np.c_[np.sin(theta)*np.cos(phi),np.full(len(phi),np.cos(theta)),np.sin(theta)*np.sin(phi)]
            area=wx*wy*(yb-ya)*np.pi/h*np.sin(theta)
            coeff+=sh(d,FULL).T@(E*area[:,None])
    return coeff


def frame(row):
    N,V=row[:3],row[3:6];nv=float(np.clip(N@V,0,1));X=V-N*nv
    if np.linalg.norm(X)<1e-8:X=np.cross([0,0,1] if abs(N[2])<.9 else [1,0,0],N)
    X=norm(X);return np.column_stack([X,np.cross(N,X),N]),nv


def rotation_grid():
    # Integrates products of degree <=7 exactly (up to FP roundoff).
    z,w=np.polynomial.legendre.leggauss(8);p=2*np.pi*(np.arange(16)+.5)/16
    d=np.stack(np.broadcast_arrays(np.sqrt(1-z*z)[:,None]*np.cos(p),np.sqrt(1-z*z)[:,None]*np.sin(p),z[:,None]),axis=-1).reshape(-1,3)
    return d,np.repeat(w,16)*2*np.pi/16


def interpolate(table,nv,rough):
    def interval(nodes,v):
        v=np.clip(v,nodes[0],nodes[-1]); i=np.clip(np.searchsorted(nodes,v)-1,0,len(nodes)-2)
        return i,np.float32((v-nodes[i])/(nodes[i+1]-nodes[i]))
    i,u=interval(NV,nv);j,v=interval(ROUGH,max(.045,rough))
    a=table[i,j]*(np.float32(1)-u)+table[i+1,j]*u
    b=table[i,j+1]*(np.float32(1)-u)+table[i+1,j+1]*u
    return a*(np.float32(1)-v)+b*v


def run_reference(out,env,rows,method,order,name):
    target=out/(name+'.txt');inp=out/(name+'-inputs.txt')
    np.savetxt(inp,rows,fmt='%.17g')
    e=os.environ.copy();e.update(DYLD_LIBRARY_PATH=str(ROOT/'build/dependencies/mono/macos-arm64/sdk/lib'),
      HAZEL_DATA=str(out/'profile'),HAZEL_SAVE_ROOT=str(out/'saves'),HAZEL_RESOURCES=str(ROOT/'Hazel/Resources'))
    cmd=[str(ROOT/'bin/Release-macos-arm64/MigrationRenderer2DSmoke/MigrationRenderer2DSmoke'),
         '--ibl-reference'+method,str(ROOT/SOURCES[env]),str(inp),str(order),str(target)]
    t=time.monotonic()
    with (out/'oracle.log').open('a') as log:subprocess.run(cmd,check=True,env=e,stdout=log,stderr=log,timeout=900)
    print(name,round(time.monotonic()-t,3),'seconds',flush=True)
    values=np.loadtxt(target,ndmin=2)[:,:3]
    if not np.all(np.isfinite(values)):raise RuntimeError('Nonfinite oracle: no row can qualify')
    return values


def references(args):
    out=args.output;out.mkdir(exist_ok=False);t=time.monotonic()
    rows,meta,capture=extract(args.capture)
    np.savetxt(out/'inputs.txt',rows,fmt='%.17g');write_json(out/'rows.json',meta)
    write_json(out/'capture-provenance.json',capture)
    for env in SOURCES:
        lo=np.zeros((len(rows),3));hi=lo.copy();filled=np.zeros(len(rows),bool)
        # Tiny is evaluated entirely by localized source quadrature, including
        # sharp rows; previous unconverged NDF values do not supply acceptance.
        for sharp in (False,True):
            ix=np.flatnonzero(~filled & ((rows[:,12]<.06)==sharp))
            if not len(ix):continue
            method='-sparse' if env=='tiny' else '-ndf' if sharp else ''
            orders=(32,64) if env=='tiny' else (1024,2048) if sharp else (2,4)
            for start in range(0,len(ix),256 if not sharp else 128):
                batch=ix[start:start+(256 if not sharp else 128)]
                for dst,n in zip((lo,hi),orders):dst[batch]=run_reference(out,env,rows[batch],method,n,f'{env}-new-{sharp}-{start}-{n}')
        rel=np.max(abs(hi-lo)/np.maximum(hi,.02),axis=1)
        limits=np.where(rows[:,12]<.06,.005,.001)
        bad=np.flatnonzero(rel>limits)
        if len(bad):
            print(env,'refining',len(bad),'unconverged rows',flush=True)
            # A fixed bounded refinement, no candidate result is consulted.
            for sharp in (False,True):
                ix=bad[(rows[bad,12]<.06)==sharp]
                if not len(ix):continue
                method='-sparse' if env=='tiny' else '-ndf' if sharp else ''
                orders=(64,128) if env=='tiny' else (2048,4096) if sharp else (8,16)
                for start in range(0,len(ix),64):
                    batch=ix[start:start+64]
                    for dst,n in zip((lo,hi),orders):dst[batch]=run_reference(out,env,rows[batch],method,n,f'{env}-refine-{sharp}-{start}-{n}')
        # Remaining broad source-grid errors get one independent NDF check.
        # Acceptance requires both NDF refinement AND agreement with source-16;
        # otherwise retain the unconverged pair, never select a favorable oracle.
        rel=np.max(abs(hi-lo)/np.maximum(hi,.02),axis=1)
        remaining=np.flatnonzero((rows[:,12]>=.06)&(rel>.001))
        if env!='tiny' and len(remaining):
            checks=[]
            for start in range(0,len(remaining),64):
                ix=remaining[start:start+64]
                a=run_reference(out,env,rows[ix],'-ndf',2048,f'{env}-cross-{start}-2048')
                b=run_reference(out,env,rows[ix],'-ndf',4096,f'{env}-cross-{start}-4096')
                nd=np.max(abs(a-b)/np.maximum(b,.02),axis=1)
                sd=np.max(abs(hi[ix]-b)/np.maximum(b,.02),axis=1)
                for q,i in enumerate(ix):
                    checks.append(dict(row=int(i+1),ndf_delta=float(nd[q]),source16_delta=float(sd[q])))
                    if nd[q]<=.001 and sd[q]<=.001:lo[i],hi[i]=a[q],b[q]
            write_json(out/f'{env}-cross-checks.json',checks)
        np.savez(out/f'{env}-reference.npz',low=lo,high=hi)
    manifest={'sources':{e:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for e,p in SOURCES.items()},
      'capture':str(args.capture),'input_sha256':hashlib.sha256((out/'inputs.txt').read_bytes()).hexdigest()}
    write_json(out/'manifest.json',manifest)
    write_json(out/'cost.json',dict(seconds=time.monotonic()-t,
        peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        child_peak_rss_bytes=resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss))


def fit(args):
    out=args.output;out.mkdir(exist_ok=False);t=time.monotonic()
    table=np.empty((len(NV),len(ROUGH),2,len(MODES)),np.float32)
    for i,nv in enumerate(NV):
        for j,r in enumerate(ROUGH):table[i,j]=project_kernel(nv,r,args.order).astype(np.float32)
        print('fit',i+1,'/',len(NV),round(time.monotonic()-t,2),'seconds',flush=True)
    np.save(out/'table.npy',table)
    write_json(out/'cost.json',dict(seconds=time.monotonic()-t,peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,order=args.order))


def metrics(pred,ref,delta,indices,rows):
    a,b=pred[indices],ref[indices];err=a-b;rel=err/np.maximum(b,.02);dark=err[b<.02]
    rms=lambda x:float(np.sqrt(np.mean(x*x))) if x.size else 0.
    worst=np.unravel_index(np.argmax(abs(rel)),rel.shape);at=int(indices[worst[0]])
    uncertainty=np.max(delta[indices]);converged=bool(np.all(delta[indices]<=np.where(rows[indices,12]<.06,.005,.001)))
    channel_rms=[rms(rel[:,c]) for c in range(3)]
    channel_dark_rms=[rms(err[:,c][b[:,c]<.02]) for c in range(3)]
    return dict(rows=len(indices),relative_rms=rms(rel),relative_channel_rms=channel_rms,
      relative_max=float(np.max(abs(rel))),
      absolute_rms=rms(err),absolute_max=float(np.max(abs(err))),dark_channels=int(dark.size),
      negative_candidate_channels=int(np.count_nonzero(a<0)),
      dark_rms=rms(dark),dark_channel_rms=channel_dark_rms,
      dark_max=float(np.max(abs(dark))) if dark.size else 0.,
      reference_delta_max=float(uncertainty),reference_converged=converged,
      pass_gate=bool(converged and max(channel_rms)<=.02 and np.max(abs(rel))<=.05 and max(channel_dark_rms)<=.0005 and (not dark.size or np.max(abs(dark))<=.002)),
      worst_row=at+1,worst_channel=int(worst[1]),worst_input=rows[at].tolist(),
      worst_candidate=pred[at].tolist(),worst_reference=ref[at].tolist(),worst_reference_delta=float(delta[at]))


def screen(args):
    out=args.output;out.mkdir(exist_ok=False);t=time.monotonic()
    rows=np.loadtxt(args.references/'inputs.txt');meta=json.loads((args.references/'rows.json').read_text())
    table=np.load(args.fit/'table.npy');d,w=rotation_grid();localbasis=sh(d,MODES)
    assert np.max(abs(sh(d,FULL).T@(w[:,None]*sh(d,FULL))-np.eye(64)))<1e-12
    groups={}
    for i,(r,m) in enumerate(zip(rows,meta)):
        nv=r[:3]@r[3:6];rough=r[12];metal=r[13]
        tags=['all',m['group'],'rough/'+('sharp' if rough<.06 else 'intermediate' if rough<.7 else 'rough'),
              'material/'+('dielectric' if metal<.01 else 'metal' if metal>.99 else 'mixed'),
              'view/'+('back' if nv<=0 else 'grazing' if nv<.11 else 'normal' if nv>.85 else 'intermediate')]
        if nv>0 and r[6:9]@r[3:6]>0:tags.append('primary_positive')
        for tag in tags:groups.setdefault(tag,[]).append(i)
    direct={}
    for i,m in enumerate(meta):
        if m['group'] in ('node','midpoint'):
            _,nv=frame(rows[i]);key=(round(nv,10),round(float(rows[i,12]),10))
            if key not in direct:direct[key]=project_kernel(nv,rows[i,12],args.order)
    report={};predictions={};projection_seconds={}
    for env,path in SOURCES.items():
        projection_start=time.monotonic()
        image=hdr(ROOT/path);a=source_coefficients(image,3);b=source_coefficients(image,6)
        projection_seconds[env]=time.monotonic()-projection_start
        # Preserve both environment projections; quantify their numerical impact.
        np.savez(out/f'{env}-lighting.npz',low=a,high=b)
        references=np.load(args.references/f'{env}-reference.npz');ref=references['high'];delta=np.max(abs(ref-references['low'])/np.maximum(ref,.02),axis=1)
        environment_groups=dict(groups)
        for label,mask in [('dark_region',np.max(ref,axis=1)<.02),('bright_region',np.max(ref,axis=1)>=1.)]:
            if np.any(mask):environment_groups[label]=np.flatnonzero(mask).tolist()
        pred={k:np.zeros((len(rows),3)) for k in RANKS};lowpred={k:np.zeros((len(rows),3)) for k in RANKS};directpred={k:np.zeros((len(rows),3)) for k in RANKS}
        for i,row in enumerate(rows):
            F,nv=frame(row)
            if nv<=0 or row[6:9]@row[3:6]<=0:continue
            worldbasis=sh(d@F.T,FULL)
            # Rotate complete world bands into just the retained local modes.
            rotation=localbasis.T@(w[:,None]*worldbasis)
            local=rotation@b;locallo=rotation@a
            coeff=interpolate(table,nv,row[12]);f0=.04*(1-row[13])+row[9:12]*row[13]
            key=(round(nv,10),round(float(row[12]),10))
            for k in RANKS:
                pred[k][i]=(f0*(coeff[0,:k]@local[:k])+coeff[1,:k]@local[:k])*row[14]
                lowpred[k][i]=(f0*(coeff[0,:k]@locallo[:k])+coeff[1,:k]@locallo[:k])*row[14]
                if key in direct:
                    co=direct[key];directpred[k][i]=(f0*(co[0,:k]@local[:k])+co[1,:k]@local[:k])*row[14]
        for k in RANKS:
            result={g:metrics(pred[k],ref,delta,np.array(ix),rows) for g,ix in environment_groups.items()}
            ix=np.flatnonzero(delta<=np.where(rows[:,12]<.06,.005,.001))
            result['converged_only']=metrics(pred[k],ref,delta,ix,rows)
            for kind in ('node','midpoint'):
                ix=np.array(groups[kind]);result['direct/'+kind]=metrics(directpred[k],ref,delta,ix,rows)
                result[kind]['interpolation_only_absolute_max']=float(np.max(abs(pred[k][ix]-directpred[k][ix])))
            result['all']['lighting_projection_delta_max']=float(np.max(abs(pred[k]-lowpred[k])/np.maximum(ref,.02)))
            report[f'{env}/rank{k}']=result;predictions[f'{env}_{k}']=pred[k]
            print(env,k,'RMS/max',result['all']['relative_rms'],result['all']['relative_max'],flush=True)
    np.savez(out/'predictions.npz',**predictions)
    write_json(out/'metrics.json',report)
    decision={}
    for k in RANKS:
        degree=MODES[k-1][0];world=(degree+1)**2
        storage=dict(environment_bytes=world*3*4,shared_bytes=2*len(NV)*len(ROUGH)*k*4+(len(NV)+len(ROUGH))*4)
        # Conservative texture formulation; analytic rotation has no angle LUT.
        # Explicit FP32 interpolation needs four reads for each vec4 coefficient.
        fetches=world+4*2*math.ceil(k/4)
        accuracy=all(v['pass_gate'] for key,g in report.items() if key.endswith(f'rank{k}')
                     for name,v in g.items() if not name.startswith('direct/') and name!='converged_only')
        decision[k]=dict(**storage,explicit_texture_fetches=fetches,
          offline_qualified=bool(accuracy and max(storage.values())<=1024**2),
          gpu_attempted=False,visual_gate_resolved=False)
    write_json(out/'decision.json',decision)
    write_json(out/'cost.json',dict(seconds=time.monotonic()-t,peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,order=args.order,lighting_projection_seconds=projection_seconds))


def bundle(args):
    """Small durable numerical evidence; no capture buffers or machine profiles."""
    out=args.output;out.mkdir(exist_ok=False)
    for name in ('inputs.txt','rows.json','capture-provenance.json','manifest.json'):
        shutil.copyfile(args.references/name,out/name)
    for name in ('axes-cross-checks.json',):
        if (args.references/name).exists():shutil.copyfile(args.references/name,out/name)
    for name in ('metrics.json','decision.json'):
        shutil.copyfile(args.screen/name,out/name)
    rows=np.loadtxt(args.references/'inputs.txt')
    for env in SOURCES:
        ref=np.load(args.references/f'{env}-reference.npz');hi=ref['high'];lo=ref['low']
        delta=np.max(abs(hi-lo)/np.maximum(hi,.02),axis=1)
        data=np.c_[np.arange(1,len(rows)+1),lo,hi,delta,delta<=np.where(rows[:,12]<.06,.005,.001)]
        np.savetxt(out/f'{env}-reference.csv',data,delimiter=',',fmt='%.12g',
          header='row,low_R,low_G,low_B,high_R,high_G,high_B,delta_floor_max,converged',comments='')
    predictions=np.load(args.screen/'predictions.npz')
    for key in predictions.files:
        np.savetxt(out/f'{key}.csv',predictions[key],delimiter=',',fmt='%.12g',header='R,G,B',comments='')
    table=np.load(args.fit/'table.npy')
    np.savetxt(out/'table.csv',table.reshape(-1,table.shape[-1]),delimiter=',',fmt='%.9g',
      header='12 view nodes x 9 roughness nodes x 2 kernels; 36 cosine modes, first 8/16/32 retained',comments='# ')
    metrics=json.loads((args.screen/'metrics.json').read_text())
    columns=['rows','relative_rms','relative_max','absolute_rms','absolute_max','dark_channels',
      'dark_rms','dark_max','reference_delta_max','reference_converged','pass_gate','worst_row','worst_channel']
    with (out/'groups.csv').open('x',newline='') as f:
        writer=csv.writer(f);writer.writerow(['candidate','group']+columns)
        for candidate,groups in metrics.items():
            for group,m in groups.items():writer.writerow([candidate,group]+[m[k] for k in columns])
    costs={}
    for key,path in [('reference',args.references),('fit',args.fit),('screen',args.screen)]:
        costs[key]=json.loads((path/'cost.json').read_text())
    write_json(out/'costs.json',costs)


def main():
    ap=argparse.ArgumentParser(description=__doc__);sub=ap.add_subparsers(dest='command',required=True)
    p=sub.add_parser('references');p.add_argument('--capture',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p=sub.add_parser('fit');p.add_argument('--order',type=int,choices=(128,256,512),default=256);p.add_argument('--output',type=Path,required=True)
    p=sub.add_parser('screen');p.add_argument('--references',type=Path,required=True);p.add_argument('--fit',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--order',type=int,choices=(128,256,512),default=256)
    p=sub.add_parser('bundle')
    for name in ('references','fit','screen','output'):p.add_argument('--'+name,type=Path,required=True)
    args=ap.parse_args();args.output=args.output.resolve()
    signal.signal(signal.SIGALRM,lambda *_: (_ for _ in ()).throw(TimeoutError('900 second job ceiling')))
    signal.alarm(900)
    os.chdir(ROOT);globals()[args.command](args)
    rss=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    # macOS reports bytes, Linux KiB.
    if os.uname().sysname!='Darwin':rss*=1024
    if rss>512*1024**2:raise RuntimeError('RSS budget exceeded: candidate cannot qualify')
    print('MEASURED; no production, GPU or visual qualification',flush=True)

if __name__=='__main__':main()
