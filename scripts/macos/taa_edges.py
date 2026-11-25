#!/usr/bin/env python3
"""Analytic geometric-AA gates for the native AO-free diagonal-edge fixture.

Coverage units: zero is background and one is the opaque unlit rectangle.
Residual changes subtract analytic per-frame coverage, retaining alias/flicker
rather than the intended change in geometry. Soft width is 5%-95% coverage pixel
count divided by projected perimeter. It detects recursive interpolation blur.
These local gates do not replace owner visual acceptance.
"""
import argparse
import csv
import json
from pathlib import Path
import statistics


def inspect(path):
    rows=list(csv.DictReader(Path(path).open()))
    if len(rows)!=512:raise RuntimeError('Expected eight 64-frame edge sequences')
    results=[]
    for projection in ('perspective','ortho'):
        for motion in range(4):
            selected=[r for r in rows if r['projection']==projection and int(r['path'])==motion]
            settled=[r for r in selected if int(r['frame'])>=16]
            average=lambda key,values=settled:statistics.mean(float(r[key]) for r in values)
            row=dict(projection=projection,motion=('static','camera_translation','camera_rotation','independent_object')[motion],
                offRMSE=average('off_rmse'),taaRMSE=average('on_rmse'),offResidualChange=average('off_residual_change'),
                taaResidualChange=average('on_residual_change'),acceptedFraction=average('accepted_fraction'),
                softWidth=average('soft_width_on') if 'soft_width_on' in selected[0] else None,
                referenceSoftWidth=average('soft_width_reference') if 'soft_width_reference' in selected[0] else None,
                meanHistoryWeight=average('mean_history_weight') if 'mean_history_weight' in selected[0] else None)
            row['checks']={'improvesGeometricError':row['taaRMSE']<row['offRMSE']*(.6 if motion==0 else .9)}
            if motion==0:
                early=average('on_rmse',selected[:4]);late=average('on_rmse',selected[-16:])
                row.update(earlyRMSE=early,lateRMSE=late)
                row['checks'].update(stationaryStable=row['taaResidualChange']<.02,converges=late<early*.75)
            else:row['checks']['improvesMovingResidual']=row['taaResidualChange']<row['offResidualChange']*.8
            if row['softWidth'] is not None:
                row['checks']['retainsEdgeWidth']=row['softWidth']<min(1.75,row['referenceSoftWidth']*1.75)
                row['checks']['rasterJitterMatches']=sum(int(r['jitter_raster_mismatches']) for r in selected)==0
            row['status']='PASS' if all(row['checks'].values()) else 'FAIL';results.append(row)
    return dict(scope=__doc__,criteria='Declared for the final candidate after diagnostic ablations, before final Debug/Release acceptance; mean settled frames 16-63. No universal image-quality claim.',
        status='PASS' if all(r['status']=='PASS' for r in results) else 'FAIL',sequences=results)


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('csv',type=Path);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();r=inspect(args.csv);args.output.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
    if r['status']!='PASS':raise SystemExit(1)
if __name__=='__main__':main()
