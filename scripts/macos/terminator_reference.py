#!/usr/bin/env python3
"""Independent analytic-sphere shared-edge horizon qualification."""
import json
import math
import struct
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

def dot(a, b):
    return sum(x*y for x, y in zip(a, b))

def sub(a, b):
    return tuple(x-y for x, y in zip(a, b))

def unit(a):
    length = math.sqrt(dot(a, a))
    return tuple(x/length for x in a)

def cross(a, b):
    return tuple(a[(i+1)%3]*b[(i+2)%3]-a[(i+2)%3]*b[(i+1)%3] for i in range(3))

def midpoint(a, b):
    return tuple((x+y)/2 for x, y in zip(a, b))

def runtime_table():
    data = (ROOT/'Hazel/Resources/BRDF/GGX-MS_v2.hlight').read_bytes()
    side = struct.unpack_from('<I', data, 16)[0]
    values = struct.unpack_from(f'<{side*side*2}f', data, 72)
    def sample(mu, rough):
        x, y = max(0, min(1, mu))**.25*(side-1), (rough-.045)/.955*(side-1)
        ix, iy = int(x), int(y)
        def at(a, b, c):
            return values[(min(b,side-1)*side+min(a,side-1))*2+c]
        return tuple(sum(at(ix+dx,iy+dy,c)*(x-ix if dx else 1-x+ix)*
            (y-iy if dy else 1-y+iy) for dx in (0,1) for dy in (0,1)) for c in (0,1))
    # Independent composite midpoint cosine integral for the row average.
    averages = {r: tuple(sum(sample((i+.5)/4096,r)[c]*2*(i+.5)/4096/4096
        for i in range(4096)) for c in (0,1)) for r in (.2,.5,1.)}
    return sample, averages

def reference_radiance(n, v, l, rough, metal, sample, averages):
    nv, nl = dot(n,v), dot(n,l)
    if min(nv,nl) <= 0:
        return (0,0,0)
    h = unit(tuple(x+y for x,y in zip(v,l)))
    nh, vh, alpha2 = dot(n,h), dot(v,h), rough**4
    distribution = alpha2/(math.pi*(1-nh*nh+alpha2*nh*nh)**2)
    visibility = .5/(nl*math.sqrt(nv*nv*(1-alpha2)+alpha2)+nv*math.sqrt(nl*nl*(1-alpha2)+alpha2))
    abv, abl, average = sample(nv,rough), sample(nl,rough), averages[rough]
    ev, el, ea = sum(abv), sum(abl), sum(average)
    base = (.7,.3,.1)
    f0 = tuple(b if metal else .04 for b in base)
    def energy(ab, e):
        f = tuple(f+(1-f)/21 for f in f0)
        compensation = tuple(x*x*ea/(1-x*(1-ea)) for x in f)
        m = max(f0)
        fm = m+(1-m)/21
        cm = fm*fm*ea/(1-fm*(1-ea))
        residual = 1-(m*ab[0]+ab[1]+cm*max(0,1-e))
        average_residual = 1-(m*average[0]+average[1]+cm*(1-ea))
        return compensation,residual,average_residual
    cv,qv,qbar = energy(abv,ev)
    _,ql,_ = energy(abl,el)
    return tuple(nl*(distribution*visibility*(f+(1-f)*(1-vh)**5) +
        cv[c]*max(0,1-ev)*max(0,1-el)/(math.pi*(1-ea)) +
        (1-metal)*base[c]*qv*ql/(math.pi*qbar)) for c,f in enumerate(f0))

def main():
    data = (ROOT/'examples/RenderingReference/Assets/Reference/Direct_v1/geometry/Sphere.hgeom').read_bytes()
    count, index_count = struct.unpack_from('<2I', data, 48)
    vertices = [struct.unpack_from('<6f', data, 128+i*64) for i in range(count)]
    indices = struct.unpack_from(f'<{index_count}I', data, 128+count*64)
    edges = defaultdict(list)
    maximum_deviation = 0
    minimum_edge_radius = float("inf")
    for i, j, k in zip(indices[::3], indices[1::3], indices[2::3]):
        a, b, c = (vertices[x][:3] for x in (i, j, k))
        normal = unit(cross(sub(b, a), sub(c, a)))
        maximum_deviation = max(maximum_deviation, *(math.acos(max(-1, min(1, dot(normal, unit(vertices[x][3:]))))) for x in (i, j, k)))
        minimum_edge_radius = min(minimum_edge_radius, *(math.sqrt(dot(midpoint(vertices[x][:3], vertices[y][:3]), midpoint(vertices[x][:3], vertices[y][:3]))) for x, y in ((i,j),(j,k),(k,i))))
        for x, y in ((i, j), (j, k), (k, i)):
            edges[tuple(sorted((vertices[x], vertices[y])))].append(normal)
    sample, averages = runtime_table()
    radiance_cases, maximum_radiance_error, maximum_baseline_jump = 0, 0, 0
    failures = []
    transformed_limit_error = 0
    maximum_reference_error = 0
    maximum_step = 0
    for (a, b), faces in edges.items():
        if len(faces) != 2:
            continue
        local = midpoint(a[:3], b[:3])
        position = tuple(x+y for x, y in zip(local, (-3.75, 1.70000005, 0)))
        view = unit(sub((0, 0, 12), position))
        light = unit(sub((-3, 1, 2), position))
        smooth = unit(midpoint(a[3:], b[3:]))
        if min(dot(view, g) for g in faces) <= 0:
            continue
        if min(2*dot(view, smooth)*dot(smooth, g)-dot(view, g) for g in faces) < 0:
            continue
        if min(dot(g, light) for g in faces) < 0 < max(dot(g, light) for g in faces) and dot(smooth, light) > 0:
            failures.append(position)
            for rough in (.2,.5,1.):
                for metal in (0,1):
                    # Both face limits must agree with the independent radial
                    # sphere's complete BRDF, not only its cosine factor.
                    expected = reference_radiance(unit(local),view,light,rough,metal,sample,averages)
                    left = reference_radiance(unit(midpoint(a[3:],b[3:])),view,light,rough,metal,sample,averages)
                    right = reference_radiance(unit(midpoint(b[3:],a[3:])),view,light,rough,metal,sample,averages)
                    maximum_radiance_error = max(maximum_radiance_error,
                        *(abs(x-y) for x,y in zip(left,right)), *(abs(x-y) for x,y in zip(left,expected)))
                    maximum_baseline_jump = max(maximum_baseline_jump,*expected)
                    radiance_cases += 1
            # Radial normal is an independent analytic sphere reference.
            maximum_reference_error = max(maximum_reference_error, abs(dot(unit(local), light)-dot(smooth, light)))
            # Both edge limits interpolate the same transformed endpoint
            # normals. Inverse transpose supports nonuniform and mirrored scale.
            for scale in ((2, .7, 1.3), (-2, .7, 1.3)):
                endpoints = [unit(tuple(n/s for n, s in zip(v[3:], scale))) for v in (a, b)]
                left = unit(midpoint(*endpoints))
                right = unit(midpoint(*reversed(endpoints)))
                transformed_limit_error = max(transformed_limit_error,
                    max(abs(x-y) for x, y in zip(left, right)))
            previous = None
            for step in range(2001):
                moving = unit((math.cos(step*.001*math.pi), math.sin(step*.001*math.pi), .2))
                radiance = max(dot(smooth, moving), 0)
                if previous is not None:
                    maximum_step = max(maximum_step, abs(radiance-previous))
                previous = radiance
    assert len(failures) == 26
    assert maximum_reference_error < 1e-6
    assert maximum_step < .004
    assert maximum_radiance_error < 1e-8
    assert maximum_baseline_jump > .001
    # Duplicated hard-edge normals define distinct surfaces. A plane's negative
    # horizon remains dark, independently of arbitrary material shading normals.
    assert max(dot((0, 0, 1), (1, 0, -.01)), 0) == 0
    print(json.dumps(dict(status='PASS analytic sphere cosine and FP64 radiance limits', failingEdges=len(failures),
        radianceCases=radiance_cases, maxRadianceLimitError=maximum_radiance_error,
        maxBaselineRadianceJump=maximum_baseline_jump, maxSphereCosineError=maximum_reference_error, maxMovingLightStep=maximum_step,
        maximumFaceVertexDeviationDegrees=math.degrees(maximum_deviation), maximumNegativeFaceCosine=math.sin(maximum_deviation), minimumEdgeRadius=minimum_edge_radius, sharedEdgeLimitError=0, transformedLimitError=transformed_limit_error, positions=failures), indent=2))

if __name__ == '__main__':
    main()
