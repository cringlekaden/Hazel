#!/usr/bin/env python3
"""CSM overlap/contact diagnostics and independent FP64 depth-grid visibility.

The baseline model describes dd85be4/878303e sampling. It is for comparing saved
captures, not an alternate renderer. Visual production acceptance stays external.
Strict stress inspection parity has known baseline failures and returns nonzero;
capturing or passing scoped reference checks does not establish production PASS.
"""
import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import statistics
import subprocess
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import hazel as hz
from macos.xegtao import pixels, png

BASELINE='878303e5270254d2e214230df0dfbf10ebf87f83'


def baseline_resources(configuration, destination, resources):
    """Historical sampling plus today's transient inspection, outside production."""
    from macos.metal_shaders import produce
    shutil.copytree(resources,destination)
    source=subprocess.check_output(['git','show',BASELINE+':Hazel/Resources/shaders/ForwardPBR.glsl'],cwd=hz.ROOT).decode()
    current=(resources/'shaders/ForwardPBR.glsl').read_text()
    inspection=current.split('vec3 shadowInspection(',1)[1].split('void main()',1)[0]
    inspection='vec3 shadowInspection('+inspection
    inspection=inspection.replace('float scale=shadowReceiverScale(depth,cascade);','float scale=u_ShadowTexelDepth[cascade].x;')
    inspection=inspection.replace('cascadeVisibility(mode-3,position,Ng,L,min(scale,u_ShadowTexelDepth[mode-3].x))','cascadeVisibility(mode-3,position,Ng,L)')
    inspection=inspection.replace('cascadeVisibility(cascade,position,Ng,L,scale)','cascadeVisibility(cascade,position,Ng,L)')
    inspection=inspection.replace('cascadeVisibility(cascade+1,position,Ng,L,scale)','cascadeVisibility(cascade+1,position,Ng,L)')
    source=source.replace('void main(){\n    // Evaluate geometric',inspection+'void main(){\n    // Evaluate geometric')
    source=source.replace('    o_Color=vec4(result,1);','    if(u_ShadowRange.w>0)result=shadowInspection(v_Position,geometricNormal,normalize(u_Light.xyz));\n    o_Color=vec4(result,1);')
    source=source.replace('float cosine=dot(Ng,L);\n    // Receiver','float cosine=dot(Ng,L);\n    float bias=u_ShadowRange.w==10?0:1;\n    // Receiver')
    source=source.replace('normal*(u_ShadowParameters.y*texel','normal*(bias*u_ShadowParameters.y*texel')
    source=source.replace('-u_ShadowParameters.z*texel*inverseDepth','-bias*u_ShadowParameters.z*texel*inverseDepth')
    (destination/'shaders/ForwardPBR.glsl').write_text(source)
    produce(configuration,destination/'shaders/metal',sources=destination/'shaders')
    return destination


def experiment_resources(configuration,destination,resources):
    """Reproduce the rejected footprint experiment, never enable it in production."""
    from macos.metal_shaders import produce
    shutil.copytree(resources,destination)
    patch=hz.ROOT/'docs/design/evidence/CSMCohesion_v1/rejected-filter.patch'
    subprocess.run(['patch','-p1','-d',str(destination/'shaders'),'-i',str(patch)],check=True)
    produce(configuration,destination/'shaders/metal',sources=destination/'shaders')
    return destination


def dot(a, b):
    return sum(x*y for x, y in zip(a, b))


def slab(origin, direction, lo, hi):
    enter, leave = 0., math.inf
    for p, d, a, b in zip(origin, direction, lo, hi):
        if abs(d) < 1e-12:
            if not a <= p <= b:
                return None
        else:
            near, far = sorted(((a-p)/d, (b-p)/d))
            enter, leave = max(enter, near), min(leave, far)
    return enter if enter <= leave else None


class Reference:
    """Ray-trace texel-centre depths, then integrate visibility cell areas.

    Uses measured projection matrices as inputs, not the production fitting,
    depth buffer, or PCF helper. Existing FP64 corner/grid tests qualify fitting.
    All geometry/intersections/area integrations here use Python FP64.
    """
    def __init__(self, meta, projections, model):
        self.meta, self.model = meta, model
        self.resolution = int(meta[0]['resolution'])
        self.splits = [float(m['split']) for m in meta]
        self.texels = [float(m['texel']) for m in meta]
        angle = float(meta[0]['slope'])
        self.s, self.c = math.sin(angle), math.cos(angle)
        self.normal = (-self.s, self.c, 0.)
        self.matrices = projections
        self.bases = []
        self.cache = {}
        for matrix in projections:
            rows = [tuple(matrix[c*4+r] for c in range(3)) for r in range(3)]
            lengths = [math.sqrt(dot(r, r)) for r in rows]
            right = tuple(x/lengths[0] for x in rows[0])
            up = tuple(x/lengths[1] for x in rows[1])
            light = tuple(-x/lengths[2] for x in rows[2])
            self.bases.append((right, up, light, lengths))

    def local(self, p):
        return (self.c*p[0]+self.s*p[1], -self.s*p[0]+self.c*p[1], p[2])

    def blocker(self, cascade, x, y):
        key = (cascade, x, y)
        if key in self.cache:
            return self.cache[key]
        m = self.matrices[cascade]
        right, up, light, lengths = self.bases[cascade]
        qx = ((x+.5)/self.resolution*2-1-m[12])/lengths[0]
        qy = ((y+.5)/self.resolution*2-1-m[13])/lengths[1]
        hi = (1+m[14])/lengths[2]
        origin = tuple(right[i]*qx+up[i]*qy+light[i]*hi for i in range(3))
        local_origin = self.local(origin)
        direction = self.local(tuple(-v for v in light))
        hits = [slab(local_origin, direction, lo, upper) for lo, upper in (
            ((-50., -.1, -125.), (50., 0., 55.)),
            ((-.1, 0., -125.), (.1, 4., 55.)))]
        hits = [h for h in hits if h is not None]
        result = hi-min(hits) if hits else -math.inf
        self.cache[key] = result
        return result

    def scale(self, depth, cascade):
        if cascade == 0:
            return self.texels[0]
        t = max(0., min(1., (depth-self.splits[cascade-1])/(self.splits[cascade]-self.splits[cascade-1])))
        return self.texels[cascade-1]*(1-t)+self.texels[cascade]*t

    def visibility(self, cascade, position, receiver_scale, coverage_bound=0.):
        m = self.matrices[cascade]
        right, up, light, _ = self.bases[cascade]
        scale = self.texels[cascade] if self.model == 'baseline' else receiver_scale
        cosine = dot(self.normal, light)
        normal_bias = float(self.meta[0]['normalBias'])*scale*math.sqrt(max(0., 1-cosine*cosine))
        offset = tuple(p+n*normal_bias for p, n in zip(position, self.normal))
        clip = [dot(tuple(m[c*4+r] for c in range(3)), offset)+m[12+r] for r in range(3)]
        if any(abs(v) > 1 for v in clip):
            return (1.,1.,1.)
        centre = [(v*.5+.5)*self.resolution for v in clip[:2]]
        width = 3*scale/self.texels[cascade]
        bounds = [(p-width*.5, p+width*.5) for p in centre]
        denominator = cosine if abs(cosine) > .05 else math.copysign(.05, cosine)
        slope = [max(-8., min(8., dot(self.normal, axis)/denominator)) for axis in (right, up)]
        result, lower, upper, area = 0., 0., 0., width*width
        # Enumerate rectangle/cell intersections, independently of the shader's
        # fixed 4x4 indices and separable weights.
        for y in range(math.floor(bounds[1][0]), math.ceil(bounds[1][1])):
            height = min(y+1, bounds[1][1])-max(y, bounds[1][0])
            for x in range(math.floor(bounds[0][0]), math.ceil(bounds[0][1])):
                weight = height*(min(x+1, bounds[0][1])-max(x, bounds[0][0]))/area
                xx, yy = [max(0, min(self.resolution-1, v)) for v in (x, y)]
                displacement = ((xx+.5-centre[0])*self.texels[cascade], (yy+.5-centre[1])*self.texels[cascade])
                receiver = dot(light, offset)-dot(slope, displacement)+float(self.meta[0]['depthBias'])*scale
                visible=receiver >= self.blocker(cascade, xx, yy)
                result += weight*visible
                classifications=[visible]
                if coverage_bound:
                    for dx in (-coverage_bound,0.,coverage_bound):
                        for dy in (-coverage_bound,0.,coverage_bound):
                            shifted=receiver-(slope[0]*dx+slope[1]*dy)*self.texels[cascade]
                            classifications.append(shifted >= self.blocker(cascade,xx+dx,yy+dy))
                lower+=weight*all(classifications);upper+=weight*any(classifications)
        return result,lower,upper


def inspect(folder, model):
    rows = list(csv.DictReader((folder/'cascades.csv').open()))
    projection_rows = list(csv.DictReader((folder/'projections.csv').open()))
    report = []
    for case in sorted({int(r['case']) for r in rows}):
        meta = [r for r in rows if int(r['case']) == case]
        projections = [[float(r['m'+str(i)]) for i in range(16)] for r in projection_rows if int(r['case']) == case]
        reference = Reference(meta, projections, model)
        positions = pixels(folder/f'boundary-{case}-position.bin')
        pair = pixels(folder/f'boundary-{case}-7.bin')
        unbiased = pixels(folder/f'boundary-{case}-unbiased.bin')
        final = pixels(folder/f'boundary-{case}-1.bin')
        data = pixels(folder/f'boundary-{case}-8.bin')
        raw = [pixels(folder/f'boundary-{case}-{mode}.bin') for mode in (3, 4, 5, 6)]
        by_boundary = {c: [] for c in range(3)}
        errors, bounded_errors, ambiguous, references, worst = [], [], [], 0, None
        for i, (p, v, d) in enumerate(zip(positions, pair, data)):
            if p[4] != 11 or abs(reference.local(p)[1]) > .002:
                continue
            cascade = next((c for c, end in enumerate(reference.splits) if d[2] <= end), 3)
            if cascade < 3 and .001 < v[2] < .999:
                by_boundary[cascade].append((abs(v[0]-v[1]), abs(unbiased[i][0]-unbiased[i][1])))
            if i % 127 == 0 and d[2] < reference.splits[3]:
                scale = reference.scale(d[2], cascade)
                for c in (cascade, min(3, cascade+1)):
                    # Raw individual-map inspection limits the scale to that
                    # map's native texel width, so its 4x4 footprint stays valid.
                    expected,lower,upper = reference.visibility(c, p[:3], min(scale, reference.texels[c]))
                    error=abs(expected-raw[c][i][0]);errors.append(error); references += 1
                    if error>.02:
                        _,lower,upper=reference.visibility(c,p[:3],min(scale,reference.texels[c]),1/256)
                        ambiguous.append(dict(pixel=[i%1024,i//1024],cascade=c,exactError=error,interval=[lower,upper],actual=raw[c][i][0]))
                    bounded_errors.append(max(lower-raw[c][i][0],raw[c][i][0]-upper,0.))
                    if worst is None or error>worst['error']:
                        worst=dict(pixel=[i%1024,i//1024],position=p[:3],cascade=c,scale=scale,expected=expected,actual=raw[c][i][0],error=error)
        lx = -.6*reference.c+float(meta[0]['lightY'])*reference.s
        ly = .6*reference.s+float(meta[0]['lightY'])*reference.c
        analytic_edge = .1-4*lx/ly
        edges = []
        for y in range(512):
            samples = [(reference.local(positions[i])[0], pair[i], final[i][0], data[i][2])
                       for i in range(y*1024, (y+1)*1024)
                       if positions[i][4] == 11 and abs(reference.local(positions[i])[1]) < .002]
            samples.sort()
            def crossing(channel, level):
                hits = []
                for a, b in zip(samples, samples[1:]):
                    va = a[1][channel] if channel < 2 else a[2]
                    vb = b[1][channel] if channel < 2 else b[2]
                    if va < level <= vb and b[0]-a[0] < 2:
                        t = (level-va)/(vb-va)
                        x = a[0]+t*(b[0]-a[0])
                        hits.append((abs(x-analytic_edge), x, a[3]+t*(b[3]-a[3])))
                return min(hits) if hits else None
            a, b, actual = [crossing(ch, .5) for ch in range(3)]
            if not (a and b and actual):
                continue
            depth = actual[2]
            cascade = next((c for c, end in enumerate(reference.splits) if depth <= end), 3)
            begin = reference.splits[cascade-1] if cascade else .1
            end = reference.splits[cascade]
            if cascade >= 3 or depth <= end-(end-begin)*.1:
                continue
            widths = []
            for channel in (0, 1):
                low, high = crossing(channel, .1), crossing(channel, .9)
                widths.append(high[1]-low[1] if low and high else None)
            # World ground-x footprint of one coarser texel, including the
            # light-ray projection onto the receiver plane.
            right, up, _, _ = reference.bases[cascade+1]
            projections_x = [reference.local(axis)[0]-reference.local(axis)[1]*lx/ly for axis in (right, up)]
            allowance = reference.texels[cascade+1]*sum(abs(x) for x in projections_x)
            scale = reference.scale(depth, cascade)
            bias_allowance = float(meta[0]['normalBias'])*scale*abs(lx/ly)
            edges.append(dict(row=y, cascade=cascade, depth=depth, current50=a[1], next50=b[1],
                              difference50=abs(a[1]-b[1]), analytic50=analytic_edge, actual50=actual[1],
                              width10to90=widths, contactAllowance=allowance+bias_allowance))
        boundary_metrics = {}
        for cascade, values in by_boundary.items():
            boundary_metrics[cascade] = dict(pixels=len(values),
                meanVisibilityDisagreement=sum(v[0] for v in values)/max(1, len(values)),
                maxVisibilityDisagreement=max((v[0] for v in values), default=0.),
                unbiasedMeanDisagreement=sum(v[1] for v in values)/max(1, len(values)))
        record = dict(case=case, settings=meta[0], boundaries=boundary_metrics, edges=edges,
                      fp64Probes=references, maxFP64VisibilityError=max(errors, default=0.),
                      maxErrorOutsideRasterInterval=max(bounded_errors,default=0.),rasterAmbiguousProbes=ambiguous,worstProbe=worst,
                      contactStatus='PASS' if edges and all(abs(e['actual50']-e['analytic50']) <= e['contactAllowance'] for e in edges) else 'INCOMPLETE' if not edges else 'FAIL')
        report.append(record)
        for mode in (1, 2, 7):
            f = folder/f'boundary-{case}-{mode}.bin'
            png(f.with_suffix('.png'), 1024, 512, pixels(f))
        print(folder.name, case, 'FP64', record['maxFP64VisibilityError'], 'contact', record['contactStatus'], flush=True)
    (folder/'continuity.json').write_text(json.dumps(report, indent=2)+'\n')
    for file in folder.glob('study-*.bin'):
        values=pixels(file)
        if len(values)!=1280*720 or not all(math.isfinite(p[c]) for p in values for c in (0,1,2,3,5)):
            raise RuntimeError('Incomplete/nonfinite combined study capture')
        png(file.with_suffix('.png'),1280,720,values)
    if any(r['maxErrorOutsideRasterInterval'] > .02 or r['contactStatus'] != 'PASS' or not r['fp64Probes'] for r in report):
        raise RuntimeError('CSM continuity/reference check failed; see continuity.json')
    return report


def compare(before,after):
    result={}
    for backend in ('metal','opengl'):
        if not (after/backend/'continuity.json').is_file():continue
        a=json.loads((before/backend/'continuity.json').read_text())
        b=json.loads((after/backend/'continuity.json').read_text())
        if len(a)!=len(b) or any(x['settings']!=y['settings'] for x,y in zip(a,b)):
            raise RuntimeError('Comparison settings/viewpoints do not match')
        for record in a:
            name='boundary-'+str(record['case'])+'-position.bin'
            if hashlib.sha256((before/backend/name).read_bytes()).digest()!=hashlib.sha256((after/backend/name).read_bytes()).digest():
                raise RuntimeError('Comparison world-position/coverage capture changed')
        measures={}
        for name,records in (('before',a),('after',b)):
            edges=[e for r in records for e in r['edges']]
            widths=[e['width10to90'] for e in edges if all(w is not None and w>0 for w in e['width10to90'])]
            bands=[v for r in records for v in r['boundaries'].values()]
            measures[name]=dict(overlapGroundPixels=sum(v['pixels'] for v in bands),
                meanMapDisagreement=sum(v['pixels']*v['meanVisibilityDisagreement'] for v in bands)/sum(v['pixels'] for v in bands),
                contourRows=len(edges),meanContour50Disagreement=statistics.mean(e['difference50'] for e in edges),
                maxContour50Disagreement=max(e['difference50'] for e in edges),
                meanAnalyticEdgeError=statistics.mean(abs(e['actual50']-e['analytic50']) for e in edges),
                maxAnalyticEdgeError=max(abs(e['actual50']-e['analytic50']) for e in edges),
                medianCoarseFineWidthRatio=statistics.median(w[1]/w[0] for w in widths),
                meanWidthDifference=statistics.mean(abs(w[1]-w[0]) for w in widths))
        result[backend]=measures
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', choices=('Debug', 'Release'), required=True)
    parser.add_argument('--resources', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--backend', choices=('metal', 'opengl', 'both'), default='both')
    parser.add_argument('--model', choices=('baseline', 'continuous'), default='baseline')
    parser.add_argument('--baseline', action='store_true',help='Build an isolated historical-sampling resource set; no production source changes')
    parser.add_argument('--experiment',action='store_true',help='Build isolated resources for the rejected continuous-footprint experiment')
    parser.add_argument('--inspect-only', action='store_true')
    parser.add_argument('--capture-only',action='store_true',help='Record fixtures without running Python references/parity; status stays NOT QUALIFIED')
    parser.add_argument('--compare',type=Path,help='Matching baseline evidence directory; writes comparison.json')
    args = parser.parse_args()
    if args.capture_only and args.inspect_only:parser.error('Choose capture or inspection')
    if args.baseline and args.experiment:parser.error('Choose baseline or experiment')
    out = args.output.resolve(); out.mkdir(parents=True, exist_ok=True)
    if not args.inspect_only and any(out.iterdir()):
        raise RuntimeError('Use a fresh evidence directory')
    if args.baseline:
        if args.inspect_only:
            parser.error('--baseline requires a fresh capture; use --model baseline to inspect saved evidence')
        args.resources=baseline_resources(args.config,out/'BaselineResources',args.resources.resolve())
        args.model='baseline'
    elif args.experiment:
        if args.inspect_only:parser.error('--experiment requires a fresh capture')
        args.resources=experiment_resources(args.config,out/'ExperimentResources',args.resources.resolve())
        args.model='continuous'
    env = os.environ.copy()
    env.update(DYLD_LIBRARY_PATH=str(hz.mono_prefix()/'lib'), HAZEL_RESOURCES=str(args.resources.resolve()),
               HAZEL_RENDERING_REFERENCE_PROJECT=str(hz.ROOT/'examples/RenderingReference/RenderingReference.hproj'),
               HAZEL_DATA=str(out/'profile'), HAZEL_SHADOW_CONTINUITY='1', MTL_DEBUG_LAYER='1', MTL_SHADER_VALIDATION='1')
    report = dict(head=hz.output(['git', 'rev-parse', 'HEAD']), baseline=BASELINE if args.model=='baseline' else None, model=args.model, configuration=args.config,
                  sourceSHA256=hashlib.sha256((args.resources/'shaders/ForwardPBR.glsl').read_bytes()).hexdigest(), backends={})
    try:
        for backend in (('metal', 'opengl') if args.backend == 'both' else (args.backend,)):
            folder = out/backend
            cmd = [hz.binaries(args.config)/'MetalRenderer2DQuadSmoke/MetalRenderer2DQuadSmoke', args.resources.resolve(), folder, '--shadows'] if backend == 'metal' else [hz.binaries(args.config)/'MigrationRenderer2DSmoke/MigrationRenderer2DSmoke', '--shadows', folder]
            if not args.inspect_only:
                with (out/(backend+'.log')).open('w') as log:
                    result = subprocess.run(list(map(str, cmd)), cwd=hz.ROOT, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=600)
                if result.returncode:
                    raise RuntimeError(backend+' capture failed; inspect log')
            if args.capture_only:
                report['backends'][backend]=dict(status='CAPTURED; Python references/parity NOT RUN')
                continue
            records = inspect(folder, args.model)
            report['backends'][backend] = dict(cases=len(records), probes=sum(r['fp64Probes'] for r in records),
                maxFP64VisibilityError=max(r['maxFP64VisibilityError'] for r in records),
                maxErrorOutsideRasterInterval=max(r['maxErrorOutsideRasterInterval'] for r in records),
                rasterAmbiguousProbes=sum(len(r['rasterAmbiguousProbes']) for r in records),
                contacts={s: sum(r['contactStatus'] == s for r in records) for s in ('PASS', 'FAIL', 'INCOMPLETE')})
        if args.compare:
            (out/'comparison.json').write_text(json.dumps(compare(args.compare.resolve(),out),indent=2)+'\n')
        if args.backend=='both' and not args.capture_only:
            parity=[]
            for file in sorted((out/'metal').glob('boundary-*-7.bin')):
                a,b=pixels(file),pixels(out/'opengl'/file.name)
                parity.append(dict(name=file.stem,idMismatches=sum(x[4]!=y[4] for x,y in zip(a,b)),
                    maxDepth=max(abs(x[5]-y[5]) for x,y in zip(a,b)),
                    maxRGB=max(abs(x[c]-y[c]) for x,y in zip(a,b) for c in range(3))))
            report['parity']=parity
            if any(p['idMismatches'] or p['maxDepth']>1e-6 or p['maxRGB']>.003 for p in parity):
                raise RuntimeError('Overlap backend parity failed')
        report['status'] = 'CAPTURED; NOT QUALIFIED' if args.capture_only else 'PASS scoped FP64 visibility/contact gates; owner visual acceptance separate'
    except Exception as error:
        report.update(status='INCOMPLETE', error=str(error)); raise
    finally:
        (out/'results.json').write_text(json.dumps(report, indent=2)+'\n')


if __name__ == '__main__':
    main()
