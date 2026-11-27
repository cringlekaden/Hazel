#!/usr/bin/env python3
"""Build the HDR bloom study from an accepted SSR Gallery and run native checks."""
import argparse
import csv
import json
import os
from pathlib import Path
import re
import shutil
import statistics
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import hazel as hz
from macos import reflection_diagnostic
from macos.screen_reflections import native_command, RESOURCES
from macos.reflection_probes import qualify_archive
from macos.xegtao import pixels, png

PROJECT = "HDR Bloom Study.hproj"
SCENE = "Scenes/HDRBloomStudy.hazel"
SIZE = (640, 360)
POSES = 17


def authored_study(source, root, runtime_only=False):
    def producer_inputs(directory, names):
        return [name for name in names if name.endswith(".hdr") or
                (Path(directory).name == "CapturedReflections" and name.endswith(".hlight"))]

    shutil.copytree(source.parent / "Assets", root / "Assets",
                    ignore=producer_inputs if runtime_only else None)
    project = source.read_text()
    source_scene = re.search(r"(?m)^  StartScene: (.+)$", project)[1]
    assets = root / "Assets"
    scene = (assets / source_scene).read_text().replace("Scene: SSR Gallery", "Scene: HDR Bloom Study", 1)
    scene = re.sub(r"(?m)^  Exposure: .*$", """  Exposure: -0.75
  TemporalAA: true
  Bloom:
    Enabled: true
    Intensity: 0.085
    Radius: 0.56
  AmbientOcclusion:
    Enabled: true
    Quality: 2
    Radius: 0.5
    Strength: 1
    Falloff: 0.615
  Shadows:
    Enabled: true
    Resolution: 512
    Distance: 30
    SplitLambda: 0.65
    NormalBias: 0.2
    DepthBias: 0.05
    Blend: 0.1""", scene, count=1)
    scene = scene.replace("      Tag: Thin warm upright\n", "      Tag: Moving HDR tube\n", 1)
    scene = scene.replace("      Translation: [-3.1, 0.8, 3.1]", "      Translation: [-3.1, 1.18, 1.8]")
    scene = scene.replace("      Scale: [0.08, 1.6, 0.12]", "      Scale: [0.12, 1.9, 0.12]")
    materials = assets / "ReflectionStudy/Materials"
    amber = (materials / "Warm panel 3.hmat").read_text()
    amber = amber.replace("Name: Warm panel 3", "Name: Amber HDR tube")
    # Leave the gallery panels at their original radiance so the accepted
    # probe bakes remain representative. Only the moving tube is new.
    before, sep, after = scene.partition("      Tag: Moving HDR tube\n")
    if not sep:
        raise RuntimeError("Expected a prepared SSR Gallery")
    scene = before + sep + after.replace("Materials/Warm panel 3.hmat", "Materials/Amber HDR tube.hmat", 1)
    (materials / "Amber HDR tube.hmat").write_text(amber.replace("  Metallic:", "  EmissiveStrength: 18\n  Metallic:", 1))
    cool = (materials / "Cool panel 3.hmat").read_text().replace("Name: Cool panel 3", "Name: Ice HDR strip")
    (materials / "Ice HDR strip.hmat").write_text(cool.replace("  Metallic:", "  EmissiveStrength: 10\n  Metallic:", 1))
    before, sep, after = scene.partition("      Tag: Thin cool upright\n")
    if not sep:
        raise RuntimeError("Expected the cool upright in the SSR Gallery")
    scene = before + sep + after.replace("Materials/Cool panel 3.hmat", "Materials/Ice HDR strip.hmat", 1)

    order = len(re.findall(r"(?m)^  - Entity:", scene))
    entities = [
        ("Sun across the open court", [0, 5, 0], [-1.04, -.93, 0], [1, 1, 1], """    DirectionalLightComponent:
      Color: [1, 0.91, 0.74]
      Intensity: 5
      Enabled: true
"""),
        ("Roof over the dark room", [-3.0, 2.75, 0], [0, 0, 0], [6.0, .15, 6.0], """    MeshRendererComponent:
      Version: 1
      Mesh: Native/Direct_v1/UnitCube.hmesh
      Materials:
        - Slot: '0000000000000007'
          Material: ReflectionStudy/Materials/Graphite.hmat
"""),
        ("Warm shadow-casting fill", [-2.9, 2.2, 2.8], [0, 0, 0], [1, 1, 1], """    PointLightComponent:
      Color: [1, 0.46, 0.18]
      Intensity: 15
      Enabled: true
      Range: 7
      CastShadows: true
      ShadowResolution: 256
"""),
        ("Blue shadow-casting spotlight", [2.0, 3.8, 1.8], [-1.04, 0, 0], [1, 1, 1], """    SpotLightComponent:
      Color: [0.27, 0.55, 1]
      Intensity: 28
      Enabled: true
      Range: 8
      InnerCone: 0.34
      OuterCone: 0.62
      CastShadows: true
      ShadowResolution: 512
"""),
        ("Glossy ivory dielectric", [2.65, .52, 1.5], [0, 0, 0], [1, 1, 1], """    MeshRendererComponent:
      Version: 1
      Mesh: Reference/Direct_v1/Sphere.hmesh
      Materials:
        - Slot: '000000000000000e'
          Material: ReflectionStudy/Materials/Glossy ivory.hmat
"""),
    ]
    ivory = (materials / "Satin metal.hmat").read_text().replace("Name: Satin metal", "Name: Glossy ivory")
    ivory = re.sub(r"(?m)^  (BaseColor|Metallic|Roughness): .+$",
                   lambda m: {"BaseColor": "  BaseColor: [.77, .71, .59, 1]",
                              "Metallic": "  Metallic: 0", "Roughness": "  Roughness: .13"}[m[1]], ivory)
    (materials / "Glossy ivory.hmat").write_text(ivory)
    for i, (name, position, rotation, scale, components) in enumerate(entities):
        scene += f"""  - Entity: {2000 + i}
    Relationship:
      Parent: 0
      Order: {order + i}
    TagComponent:
      Tag: {name}
    TransformComponent:
      Translation: {position}
      Rotation: {rotation}
      Scale: {scale}
{components}"""
    (assets / SCENE).write_text(scene)
    project = re.sub(r"(?m)^  Name: .*$", "  Name: HDR Bloom Study", project, count=1)
    result = root / PROJECT
    result.write_text(project.replace(source_scene, SCENE))
    return result


def timings(log, destination, backend):
    feedback = {int(serial): (float(end) - float(start)) * 1000 for serial, start, end in re.findall(
        r"OFFSCREEN_FEEDBACK frame=(\d+) gpu_start=([\d.]+) gpu_end=([\d.]+)", log)}
    entries = {}
    phases = {}
    for row in log.splitlines():
        fields = row.split()
        if row.startswith("BLOOM_PHASES "):
            w, h, sample = map(int, fields[1:4])
            phases[(f"{w}x{h}", sample)] = list(map(float, fields[4:]))
            continue
        if not row.startswith("BLOOM_TIMING "):
            continue
        w, h, on, sample, serial, ns = map(int, fields[1:7])
        duration = feedback[serial] if backend == "metal" else ns / 1e6
        if duration <= 0:
            raise RuntimeError("Invalid native frame timing")
        entries.setdefault(f"{w}x{h}", [{}, {}])[on][sample] = [duration, *map(float, fields[7:])]
    with destination.open("w", newline="") as stream:
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(("extent", "on", "sample", "frameMS", "extractMS", "downMS", "upMS", "presentMS", "bytes"))
        for name, states in entries.items():
            for on, samples in enumerate(states):
                for sample, values in samples.items():
                    if backend == "opengl" and on:
                        values.extend(phases[(name, sample)])
                    if on and (len(values) != 6 or min(values[1:5]) <= 0):
                        raise RuntimeError(f"Invalid GPU phase sample for {backend} {name}, sample {sample}")
                    writer.writerow((name, on, sample, *values))
    if len(entries) != 3 or any(len(state) != 15 for pair in entries.values() for state in pair):
        raise RuntimeError("Incomplete timing samples")
    def metric(values):
        return {"median": statistics.median(values), "p95": max(values)}
    return {name: {"offFrameMS": metric([r[0] for r in off.values()]),
                   "onFrameMS": metric([r[0] for r in on.values()]),
                   "pairedFrameDeltaMS": metric([on[i][0] - off[i][0] for i in on]),
                   **{label: metric([r[i] for r in on.values()]) for i, label in
                      enumerate(("extractMS", "downMS", "upMS", "presentMS"), start=1)},
                   "scratchBytes": int(next(iter(on.values()))[-1])}
            for name, (off, on) in entries.items()}


def compare(output):
    metal_on, metal_off, gl_on, gl_off = [pixels(output / backend / f"parity-{mode}.bin")
        for backend in ("metal", "opengl") for mode in ("on", "off")]
    values = []
    id_differences = 0
    for m1, m0, g1, g0 in zip(metal_on, metal_off, gl_on, gl_off):
        if m1[4] != g1[4]:
            id_differences += 1
            continue
        values.append(max(abs((m1[i] - m0[i]) - (g1[i] - g0[i])) for i in range(3)))
    values.sort()
    error = values[int(.99 * len(values))]
    if error > .005 or id_differences > 460:
        raise RuntimeError("AA-off bloom increment or IDs exceeded native parity bounds")
    return {"bloomIncrementRGBp99": error, "idDifferences": id_differences, "compared": len(values)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--from-gallery", type=Path, help="Prepared SSR Gallery project")
    source.add_argument("--project", type=Path, help="Existing HDR Bloom Study project")
    source.add_argument("--archive", type=Path, help="Qualify an exported Nutella HDR Bloom Study")
    parser.add_argument("--offscreen-only", action="store_true", help="Archive validation without desktop presentation")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--config", choices=("Debug", "Release"), default="Release")
    parser.add_argument("--backend", choices=("both", "metal", "opengl"), default="both")
    parser.add_argument("--resources", type=Path, default=RESOURCES)
    parser.add_argument("--prepare-only", action="store_true")
    parser.add_argument("--runtime-only", action="store_true",
                        help="Omit producer inputs from the generated study for archive qualification")
    parser.add_argument("--benchmark", action="store_true")
    parser.add_argument("--motion", action="store_true")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    if args.archive:
        report = {"status": "INCOMPLETE"}
        try:
            qualify_archive(args.archive.resolve(), output, report, args.offscreen_only, hdr_bloom=PROJECT)
            report["status"] = "PASS native HDR bloom archive relocation"
        finally:
            (output / "results.json").write_text(json.dumps(report, indent=2) + "\n")
        return
    if args.offscreen_only:
        parser.error("--offscreen-only requires --archive")
    if args.runtime_only and not args.from_gallery:
        parser.error("--runtime-only requires --from-gallery")
    project = args.project.resolve() if args.project else authored_study(args.from_gallery.resolve(), output / "HDR Bloom Study", args.runtime_only)
    print(project, flush=True)
    if args.prepare_only:
        return
    env = {k: v for k, v in os.environ.items() if not k.startswith(("HAZEL_", "MTL_", "DYLD_"))}
    env.update(HAZEL_HDR_BLOOM="1", HAZEL_REFLECTION_DIAGNOSTIC="1", HAZEL_RENDERING_REFERENCE_PROJECT=str(project),
               HAZEL_RESOURCES=str(args.resources.resolve()), DYLD_LIBRARY_PATH=str(hz.mono_prefix() / "lib"))
    if args.benchmark:
        env.update(HAZEL_BLOOM_BENCHMARK="1", HAZEL_METAL_TRACE="1")
    else:
        env.update(MTL_DEBUG_LAYER="1", MTL_SHADER_VALIDATION="1")
    report = {"config": args.config, "project": str(project), "status": "INCOMPLETE", "backends": {}}
    try:
        for backend in ("metal", "opengl") if args.backend == "both" else (args.backend,):
            folder = output / backend
            log_path = output / (backend + ".log")
            cmd = native_command(args.config, backend, args.resources.resolve(), folder)
            trace_path = output / (backend + "-stderr.log")
            with log_path.open("w") as log, trace_path.open("w") as trace:
                run = subprocess.run(cmd, cwd=hz.ROOT, env={**env, "HAZEL_DATA": str(folder) + "-profile"},
                                     stdout=log, stderr=trace, timeout=600)
            log = log_path.read_text() + "\n" + trace_path.read_text()
            if run.returncode:
                raise RuntimeError(f"{backend} failed. See {log_path}")
            record = {"command": list(map(str, cmd)), "checks": [line for line in log.splitlines()
                      if line.startswith("BLOOM_") and not line.startswith(("BLOOM_TIMING", "BLOOM_PHASES"))]}
            report["backends"][backend] = record
            if args.benchmark:
                record["timing"] = timings(log, folder / "timing.csv", backend)
                continue
            for state in ("off", "on"):
                png(folder / (state + ".png"), *SIZE, pixels(folder / f"{state}-{POSES // 2}.bin"))
            if args.motion:
                reflection_diagnostic.write_motion(folder / "matched-motion.png", folder, pixels,
                                                   modes=("off", "on"), poses=POSES)
        if not args.benchmark and args.backend == "both":
            report["aaOffParity"] = compare(output)
        report["status"] = "PASS focused HDR bloom study"
    finally:
        (output / "results.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
