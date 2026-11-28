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


def authored_study(source, root, runtime_only=False, physical_camera=False, automatic_camera=False,
                   defocus_camera=False, shutter_camera=False, lens_camera=False, grade_camera=False):
    def producer_inputs(directory, names):
        return [name for name in names if name.endswith(".hdr") or
                (Path(directory).name == "CapturedReflections" and name.endswith(".hlight"))]

    shutil.copytree(source.parent / "Assets", root / "Assets",
                    ignore=producer_inputs if runtime_only else None)
    project = source.read_text()
    source_scene = re.search(r"(?m)^  StartScene: (.+)$", project)[1]
    assets = root / "Assets"
    if lens_camera:
        (assets / "CameraStudy").mkdir(exist_ok=True)
        png(assets / "CameraStudy/Dirt.png", 2, 2, [(1, 1, 1)] * 4)
    if grade_camera:
        (assets / "CameraStudy").mkdir(exist_ok=True)
        table = ["LUT_3D_SIZE 17", "LUT_3D_INPUT_RANGE 0.05 0.75"]
        for b in range(17):
            for g in range(17):
                for r in range(17):
                    table.append(f"{.125 + .75 * g / 16} {.125 + .5 * b / 16} {.125 + .75 * r / 16}")
        (assets / "CameraStudy/Grade.cube").write_text("\n".join(table) + "\n")
        (assets / "CameraStudy/GradeAlt.cube").write_text("LUT_3D_SIZE 2\n" + ".8 .2 .3\n" * 8)
        (assets / "CameraStudy/Malformed.cube").write_text("LUT_3D_SIZE 17\n.5 .6 .7\n")
    scene = (assets / source_scene).read_text().replace("Scene: SSR Gallery", "Scene: HDR Bloom Study", 1)
    if physical_camera or defocus_camera:
        lens = r"""\1
        PhysicalLens:
          Version: 1
          Projection: true
          FocalLengthMM: 25
          SensorWidthMM: 36
          SensorHeightMM: 24
          Aperture: 8
          ShutterSeconds: 0.015625
          ISO: 200
          FocusDistance: 8"""
        if defocus_camera:
            lens = lens.replace("Projection: true", "Projection: false").replace("FocalLengthMM: 25", "FocalLengthMM: 85")
            lens = lens.replace("Aperture: 8", "Aperture: 1.4").replace("FocusDistance: 8", "FocusDistance: 2.8")
            lens += """
      DepthOfField:
        Version: 1
        Enabled: true
        Quality: Balanced
        ApertureBlades: 6
        ApertureRotationRadians: 0.3
        FocusHalfLifeSeconds: 0.25"""
        scene, count = re.subn(r"(?m)^(        OrthographicFar: .+)$", lens, scene, count=1)
        if count != 1:
            raise RuntimeError("The study camera is missing")
    if shutter_camera:
        marker = "      Primary: true"
        if scene.count(marker) != 1:
            raise RuntimeError("The shutter study needs one primary camera")
        scene = scene.replace(marker, """      MotionBlur:
        Version: 1
        Enabled: true
        Shutter: Angle
        ShutterAngleDegrees: 180
        Quality: Balanced
""" + marker)
    if lens_camera:
        marker = "      Primary: true"
        if scene.count(marker) != 1:
            raise RuntimeError("The lens study needs one primary camera")
        scene = scene.replace(marker, """      LensEffects:
        Version: 1
        FlareStrength: 0.18
        GhostCount: 3
        HaloStrength: 0.3
        DirtTexture: CameraStudy/Dirt.png
        DirtStrength: 0.12
        Vignette: 0.15
""" + marker)
    if grade_camera:
        marker = "      Primary: true"
        if scene.count(marker) != 1:
            raise RuntimeError("The grade study needs one primary camera")
        scene = scene.replace(marker, """      ColorGrade:
        Version: 1
        WhiteKelvin: 4500
        TintUV: 0.004
        Saturation: 0.85
        Contrast: 0.3
        Highlights: -0.2
        LUT: CameraStudy/Grade.cube
        LUTStrength: 0.14
""" + marker)
    scene, count = re.subn(r"(?m)^  (?:Exposure: .*$|CameraExposure:(?:\n    [^\n]+)*)", """  CameraExposure:
    Version: 1
    Mode: ManualPhysical
    CompensationStops: -0.75
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
    if count != 1:
        raise RuntimeError("The SSR gallery has no authored exposure")
    if automatic_camera:
        scene = scene.replace("    Mode: ManualPhysical\n", "    Mode: Automatic\n", 1)
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


def exposure_timings(log, folder, backend):
    feedback = {int(serial): (float(end) - float(start)) * 1000 for serial, start, end in re.findall(
        r"OFFSCREEN_FEEDBACK frame=(\d+) gpu_start=([\d.]+) gpu_end=([\d.]+)", log)}
    meters, frames = {}, {}
    for line in log.splitlines():
        fields = line.split()
        if line.startswith("CAMERA_METER_TIMING "):
            w, h, sample = map(int, fields[1:4])
            meters.setdefault((w, h), {})[sample] = (float(fields[4]), int(fields[5]))
        elif line.startswith("CAMERA_FRAME_TIMING "):
            w, h, automatic, sample, serial, ns = map(int, fields[1:])
            frames.setdefault((w, h), [{}, {}])[automatic][sample] = (
                feedback[serial] if backend == "metal" else ns / 1e6)
    def metric(values):
        return {"median": statistics.median(values), "p95": max(values)}
    result = {}
    ceilings = (0.25, 0.30, 0.45) if backend == "metal" else (0.45, 0.60, 0.90)
    with (folder / "exposure-timings.csv").open("w", newline="") as stream:
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(("extent", "sample", "meterMS", "scratchBytes", "manualFrameMS", "autoFrameMS"))
        for (w, h), ceiling in zip(((1280, 720), (1920, 1080), (3840, 2160)), ceilings):
            extent = f"{w}x{h}"
            meter = meters[(w, h)]
            manual, auto = frames[(w, h)]
            if any(set(samples) != set(range(15)) for samples in (meter, manual, auto)):
                raise RuntimeError(f"Incomplete {backend} exposure timing samples at {extent}")
            if min(*manual.values(), *auto.values(), *(row[0] for row in meter.values())) <= 0:
                raise RuntimeError(f"Invalid {backend} exposure timestamps at {extent}")
            meter_ms = metric([row[0] for row in meter.values()])
            scratch = max(row[1] for row in meter.values())
            if meter_ms["median"] > ceiling or meter_ms["p95"] > 2 * ceiling or scratch > 1024 * 1024:
                raise RuntimeError(f"{backend} exposure pass exceeded its budget at {extent}")
            result[extent] = {"meterMS": meter_ms, "scratchBytes": scratch,
                              "manualFrameMS": metric(manual.values()), "autoFrameMS": metric(auto.values()),
                              "pairedFrameDeltaMS": metric([auto[i] - manual[i] for i in range(15)])}
            for i in range(15):
                writer.writerow((extent, i, *meter[i], manual[i], auto[i]))
    return result


def optical_timings(log, folder, backend, effect, names, samples=15):
    feedback = {int(serial): (float(end) - float(start)) * 1000 for serial, start, end in re.findall(
        r"OFFSCREEN_FEEDBACK frame=(\d+) gpu_start=([\d.]+) gpu_end=([\d.]+)", log)}
    passes, frames = {}, {}
    for line in log.splitlines():
        fields = line.split()
        if line.startswith(effect.upper() + "_PASS_TIMING "):
            w, h, sample = map(int, fields[1:4])
            passes.setdefault((w, h), {})[sample] = (*map(float, fields[4:-1]), int(fields[-1]))
        elif line.startswith(effect.upper() + "_FRAME_TIMING "):
            w, h, enabled, sample, serial, ns = map(int, fields[1:])
            frames.setdefault((w, h), [{}, {}])[enabled][sample] = (
                feedback[serial] if backend == "metal" else ns / 1e6)
    def metric(values):
        ordered = sorted(values)
        return {"median": statistics.median(ordered), "p95": ordered[(95 * len(ordered) + 99) // 100 - 1]}
    result = {}
    with (folder / f"{effect}-timings.csv").open("w", newline="") as stream:
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(("extent", "sample", *names,
                         "scratchBytes", "disabledFrameMS", "enabledFrameMS"))
        for w, h in ((1280, 720), (1920, 1080), (3840, 2160)):
            extent = f"{w}x{h}"
            measurements = passes[(w, h)]
            disabled, enabled = frames[(w, h)]
            if any(set(phase) != set(range(samples)) for phase in (measurements, disabled, enabled)):
                raise RuntimeError(f"Incomplete {backend} {effect} timings at {extent}")
            if min(*disabled.values(), *enabled.values()) <= 0:
                raise RuntimeError(f"Invalid {backend} {effect} frame timing at {extent}")
            result[extent] = {"passesMS": metric([sum(row[:-1]) for row in measurements.values()]),
                              "scratchBytes": max(row[-1] for row in measurements.values()),
                              "offFrameMS": metric(disabled.values()), "onFrameMS": metric(enabled.values()),
                              "pairedFrameDeltaMS": metric([enabled[i] - disabled[i] for i in range(samples)])}
            for i in range(samples):
                writer.writerow((extent, i, *measurements[i], disabled[i], enabled[i]))
    return result


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
    parser.add_argument("--automatic-camera", action="store_true", help="Use the production automatic policy in the authored study")
    parser.add_argument("--physical-camera", action="store_true",
                        help="Author the study's runtime camera with a physical lens")
    parser.add_argument("--defocus-camera", action="store_true", help="Author the study's runtime camera with depth of field")
    parser.add_argument("--shutter-camera", action="store_true", help="Author the study's runtime camera with motion blur")
    parser.add_argument("--lens-camera", action="store_true", help="Author camera flare and a private dirt fixture")
    parser.add_argument("--grade-camera", action="store_true", help="Author white balance and a linear .cube fixture")
    parser.add_argument("--runtime-only", action="store_true",
                        help="Omit producer inputs from the generated study for archive qualification")
    benchmark = parser.add_mutually_exclusive_group()
    benchmark.add_argument("--benchmark", action="store_true")
    benchmark.add_argument("--exposure-benchmark", action="store_true", help="Time the production exposure meter in isolation")
    benchmark.add_argument("--defocus-study", action="store_true", help="Run the signed defocus disc fixtures")
    benchmark.add_argument("--defocus-benchmark", action="store_true", help="Time the depth-of-field passes and complete frames")
    benchmark.add_argument("--shutter-study", action="store_true", help="Run the motion shutter reference")
    benchmark.add_argument("--shutter-benchmark", action="store_true", help="Time the shutter passes and complete frames")
    benchmark.add_argument("--lens-benchmark", action="store_true", help="Time flare and the full lens-enabled frame")
    benchmark.add_argument("--grade-benchmark", action="store_true", help="Measure the incremental camera color cost")
    benchmark.add_argument("--grade-study", action="store_true", help="Run camera color references")
    benchmark.add_argument("--lens-study", action="store_true", help="Run the physical lens effect references")
    parser.add_argument("--grade-benchmark-scenario", choices=("standard", "control", "combined"), default="standard",
                        help="Measure white balance and grading, background noise, or all authored optics")
    parser.add_argument("--lens-benchmark-full", action="store_true", help="Include optical warp, grain and the maximum flare tap count")
    parser.add_argument("--motion", action="store_true")
    args = parser.parse_args()
    if args.lens_benchmark_full and not args.lens_benchmark and not args.grade_benchmark:
        parser.error("--lens-benchmark-full requires --lens-benchmark")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    if args.archive:
        report = {"status": "INCOMPLETE"}
        try:
            qualify_archive(args.archive.resolve(), output, report, args.offscreen_only, hdr_bloom=PROJECT,
                            defocus_study=args.defocus_study, shutter_study=args.shutter_study, lens_study=args.lens_study,
                            grade_study=args.grade_study)
            report["status"] = "PASS native offscreen archive relocation" if args.offscreen_only else "PASS native archive relocation"
        finally:
            (output / "results.json").write_text(json.dumps(report, indent=2) + "\n")
        return
    if args.offscreen_only:
        parser.error("--offscreen-only requires --archive")
    if args.runtime_only and not args.from_gallery:
        parser.error("--runtime-only requires --from-gallery")
    if args.automatic_camera and not args.from_gallery:
        parser.error("--automatic-camera requires --from-gallery")
    if args.physical_camera and not args.from_gallery:
        parser.error("--physical-camera requires --from-gallery")
    if args.defocus_camera and not args.from_gallery:
        parser.error("--defocus-camera requires --from-gallery")
    if args.shutter_camera and not args.from_gallery:
        parser.error("--shutter-camera requires --from-gallery")
    if args.lens_camera and not args.from_gallery:
        parser.error("--lens-camera requires --from-gallery")
    if args.grade_camera and not args.from_gallery:
        parser.error("--grade-camera requires --from-gallery")
    project = args.project.resolve() if args.project else authored_study(
        args.from_gallery.resolve(), output / "HDR Bloom Study", args.runtime_only, args.physical_camera,
        args.automatic_camera, args.defocus_camera, args.shutter_camera, args.lens_camera, args.grade_camera)
    print(project, flush=True)
    if args.prepare_only:
        return
    env = {k: v for k, v in os.environ.items() if not k.startswith(("HAZEL_", "MTL_", "DYLD_"))}
    env.update(HAZEL_HDR_BLOOM="1", HAZEL_REFLECTION_DIAGNOSTIC="1", HAZEL_RENDERING_REFERENCE_PROJECT=str(project),
               HAZEL_RESOURCES=str(args.resources.resolve()), DYLD_LIBRARY_PATH=str(hz.mono_prefix() / "lib"))
    if args.exposure_benchmark:
        env.update(HAZEL_CAMERA_EXPOSURE_BENCHMARK="1", HAZEL_METAL_TRACE="1")
    if args.defocus_study:
        env.update(HAZEL_CAMERA_DEFOCUS_TEST="1")
    if args.shutter_study:
        env.update(HAZEL_CAMERA_SHUTTER_TEST="1")
    if args.lens_study:
        env.update(HAZEL_CAMERA_LENS_TEST="1")
    if args.grade_benchmark:
        env.update(HAZEL_CAMERA_GRADE_BENCHMARK=args.grade_benchmark_scenario, HAZEL_METAL_TRACE="1")
    if args.grade_study:
        env.update(HAZEL_CAMERA_GRADE_TEST="1")
    if args.lens_benchmark:
        env.update(HAZEL_CAMERA_LENS_BENCHMARK="full" if args.lens_benchmark_full else "1", HAZEL_METAL_TRACE="1")
    if args.shutter_benchmark:
        env.update(HAZEL_CAMERA_SHUTTER_BENCHMARK="1", HAZEL_METAL_TRACE="1")
    if args.defocus_benchmark:
        env.update(HAZEL_CAMERA_DEFOCUS_BENCHMARK="1", HAZEL_METAL_TRACE="1")
    if args.benchmark:
        env.update(HAZEL_BLOOM_BENCHMARK="1", HAZEL_METAL_TRACE="1")
    elif not args.exposure_benchmark and not args.defocus_benchmark and not args.shutter_benchmark and not args.lens_benchmark and not args.grade_benchmark:
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
                      if line.startswith(("BLOOM_", "CAMERA_", "DEFOCUS_", "SHUTTER_", "LENS_", "GRADE_")) and not line.startswith(
                          ("BLOOM_TIMING", "BLOOM_PHASES", "CAMERA_METER_TIMING", "CAMERA_FRAME_TIMING",
                           "DEFOCUS_PASS_TIMING", "DEFOCUS_FRAME_TIMING", "SHUTTER_PASS_TIMING", "SHUTTER_FRAME_TIMING",
                           "LENS_PASS_TIMING", "LENS_FRAME_TIMING", "GRADE_PASS_TIMING", "GRADE_FRAME_TIMING"))]}
            report["backends"][backend] = record
            if args.exposure_benchmark:
                record["timing"] = exposure_timings(log, folder, backend)
                continue
            if args.benchmark:
                record["timing"] = timings(log, folder / "timing.csv", backend)
                continue
            if args.defocus_study:
                for state in ("near", "near-sharp", "far", "far-sharp", "stripe", "stripe-sharp"):
                    png(folder / f"defocus-{state}.png", 1280, 720, pixels(folder / f"defocus-{state}.bin"))
                for state in ("gallery-0", "gallery-sharp-0", "gallery-4", "gallery-sharp-4"):
                    png(folder / f"defocus-{state}.png", *SIZE, pixels(folder / f"defocus-{state}.bin"))
                continue
            if args.defocus_benchmark:
                record["timing"] = optical_timings(log, folder, backend, "defocus",
                                                  ("classifyMS", "dilateMS", "prefilterMS", "gatherMS"))
                continue
            if args.shutter_study:
                for state in ("motion", "cut"):
                    png(folder / f"shutter-{state}.png", 1280, 720, pixels(folder / f"shutter-{state}.bin"))
                for state in ("gallery-motion", "gallery-sharp"):
                    png(folder / f"shutter-{state}.png", *SIZE, pixels(folder / f"shutter-{state}.bin"))
                continue
            if args.shutter_benchmark:
                record["timing"] = optical_timings(log, folder, backend, "shutter",
                                                  ("classifyMS", "neighborMS", "gatherMS"), samples=31)
                continue
            if args.lens_benchmark:
                record["timing"] = optical_timings(log, folder, backend, "lens", ("flareMS",), samples=31)
                continue
            if args.grade_benchmark:
                record["timing"] = optical_timings(log, folder, backend, "grade", ("compositeDeltaMS",), samples=31)
                continue
            if args.grade_study:
                for state in ("gallery", "gallery-base"):
                    png(folder / f"grade-{state}.png", *SIZE, pixels(folder / f"grade-{state}.bin"))
                continue
            if args.lens_study:
                for state in ("flare", "dirt", "distortion", "gallery"):
                    png(folder / f"lens-{state}.png", *SIZE, pixels(folder / f"lens-{state}.bin"))
                continue
            for state in ("off", "on", "auto"):
                png(folder / (state + ".png"), *SIZE, pixels(folder / f"{state}-{POSES // 2}.bin"))
            for state in ("reference", "physical", "compensated"):
                capture = folder / f"camera-{state}.bin"
                if capture.exists():
                    png(folder / f"camera-{state}.png", *SIZE, pixels(capture))
            if args.motion:
                reflection_diagnostic.write_motion(folder / "matched-motion.png", folder, pixels,
                                                   modes=("off", "on"), poses=POSES)
        if not any((args.benchmark, args.exposure_benchmark, args.defocus_study, args.defocus_benchmark,
                    args.shutter_study, args.shutter_benchmark, args.lens_study, args.lens_benchmark, args.grade_study, args.grade_benchmark)) and args.backend == "both":
            report["aaOffParity"] = compare(output)
        if args.defocus_study:
            report["status"] = "PASS defocus disc study"
        elif args.defocus_benchmark:
            report["status"] = "MEASURED defocus timing"
        elif args.shutter_study:
            report["status"] = "PASS motion shutter study"
        elif args.shutter_benchmark:
            report["status"] = "MEASURED motion shutter timing"
        elif args.lens_benchmark:
            report["status"] = "MEASURED lens effects timing"
        elif args.grade_benchmark:
            report["status"] = "MEASURED camera color timing"
        elif args.grade_study:
            report["status"] = "PASS camera color study"
        elif args.lens_study:
            report["status"] = "PASS lens effects study"
        else:
            report["status"] = "PASS exposure timings" if args.exposure_benchmark else "PASS focused HDR bloom and camera study"
    finally:
        (output / "results.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
