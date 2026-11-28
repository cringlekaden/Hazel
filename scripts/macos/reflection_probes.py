#!/usr/bin/env python3
"""Generate and qualify Hazel's two-room static reflection study."""
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
import tempfile
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import hazel as hz
from macos import reflection_diagnostic as gallery
from macos.xegtao import pixels, png

PROJECT_NAME = "Reflection Study.hproj"
REFERENCE = hz.ROOT / "examples/RenderingReference/Assets"
RESOURCES = hz.ROOT / "Hazel/Resources"


def write_radiance(path, warm):
    width, height = 256, 128
    data = bytearray(f"#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y {height} +X {width}\n".encode())
    walls = ((.31, .12, .07), (.07, .18, .34))
    panel = (9, 4.8, 1.7) if warm else (1.8, 5, 10)
    for y in range(height):
        inclination = math.pi * (y + .5) / height
        for x in range(width):
            azimuth = 2 * math.pi * ((x + .5) / width - .5)
            ray = (math.sin(inclination) * math.cos(azimuth), math.cos(inclination),
                   math.sin(inclination) * math.sin(azimuth))
            faces = (2.5, 1.5 if ray[1] > 0 else -1.5, 2.85)
            hit_axis = min(range(3), key=lambda axis: abs(faces[axis] / ray[axis]))
            distance = abs(faces[hit_axis] / ray[hit_axis])
            hit = [d * distance for d in ray]
            rgb = walls[0 if warm else 1]
            if hit_axis == 1:
                rgb = (.29, .27, .25) if ray[1] < 0 else (.39, .38, .36)
                if ray[1] > 0 and abs(hit[0] - (.65 if warm else -.55)) < .55 and abs(hit[2] + .15) < 1:
                    rgb = panel
            elif hit_axis == 2 and ray[2] < 0 and abs(hit[0]) < .48 and abs(hit[1] - .5) < .7:
                rgb = panel
            exponent = math.frexp(max(rgb))[1]
            scale = 256 / math.ldexp(1., exponent)
            data.extend((*[int(channel * scale) for channel in rgb], exponent + 128))
    path.write_bytes(data)


def prepare(root, config, env, diagnostic=False, probe_side=256, panorama_width=512):
    root.mkdir(parents=True)
    assets = root / "Assets"
    for name in ("Reference/Direct_v1", "Native/Direct_v1"):
        shutil.copytree(REFERENCE / name, assets / name,
                        ignore=shutil.ignore_patterns("Source.generated", "*.lock"))
    ibl = assets / "Reference/IBL_v1"
    ibl.mkdir(parents=True)
    for name in ("Constant.henv", "Constant.henv.accepted", "Constant.henv.prepared"):
        item = REFERENCE / "Reference/IBL_v1" / name
        if item.is_dir():
            shutil.copytree(item, ibl / name)
        else:
            shutil.copy2(item, ibl / name)
    shutil.copytree(REFERENCE / "Prepared/BRDF", assets / "Prepared/BRDF", dirs_exist_ok=True)
    folder = assets / "ReflectionStudy"
    folder.mkdir()
    scenes = assets / "Scenes"
    scenes.mkdir()
    project = root / (gallery.LOCAL_PROJECT if diagnostic else PROJECT_NAME)
    if diagnostic:
        gallery.write_project(root)
    else:
        project.write_text("""Project:
  Version: 1
  Name: Static Reflection Study
  ScriptProject: RenderingReference
  AuthoringVersion: 1
  StartScene: Scenes/ReflectionStudy.hazel
  AssetDirectory: Assets
  ScriptModulePath: Scripts/Binaries/RenderingReference.dll
""")
        (scenes / "ReflectionStudy.hazel").write_text(study_scene())
    timings = {}
    tool = hz.binaries(config) / "HazelProject/HazelProject"
    for name in ("Warm", "Cool"):
        if diagnostic:
            gallery.write_radiance(folder / f"{name}.hdr", name == "Warm", panorama_width)
        else:
            write_radiance(folder / f"{name}.hdr", name == "Warm")
        (folder / f"{name}.henv").write_text(f"""Environment:
  Version: 1
  Name: {name} reflection room
  Notice: Procedural Hazel qualification fixture, Apache-2.0
  Source:
    Kind: RadianceHDR
    Image: ReflectionStudy/{name}.hdr
    Interpretation: LinearRec709D65TopDown
  Preparation:
    Recipe: Hazel.IBL_v1.GGX-correlated-single.r045.E-pi.AB-NoV-r.cube-top
    Background: 16
    Specular: {probe_side if diagnostic else 128}
    Irradiance: {64 if diagnostic and probe_side > 128 else 32}
    Samples: 1024
""")
        start = time.perf_counter()
        with (root / f"prepare-{name}.log").open("w") as log:
            subprocess.run([tool, "prepare-environment", project, f"ReflectionStudy/{name}.henv"],
                           check=True, env=env, stdout=log, stderr=subprocess.STDOUT)
        timings[name] = round(time.perf_counter() - start, 3)
    return timings


def study_scene():
    header = """SceneVersion: 3
Scene: Static Reflection Study
Presentation:
  Mode: SceneReferred
  Exposure: 0
  Background: [.025,.027,.035,1]
  Environment:
    Asset: Reference/IBL_v1/Constant.henv
    Yaw: 0
    Gain: 1
    Lighting: true
    Background: false
Entities:
"""
    entities = []

    def add(name, position, scale, fields):
        i = len(entities)
        entities.append(f"""  - Entity: {100 + i}
    Relationship:
      Parent: 0
      Order: {i}
    TagComponent:
      Tag: {name}
    TransformComponent:
      Translation: {list(position)}
      Rotation: [0, 0, 0]
      Scale: {list(scale)}
{fields}""")

    def mesh(name, position, scale, sphere=False, material=None):
        reference = "Reference/Direct_v1/Sphere.hmesh" if sphere else "Native/Direct_v1/UnitCube.hmesh"
        slot = "000000000000000e" if sphere else "0000000000000007"
        overrides = (f"\n        - Slot: {slot}\n          Material: Reference/Direct_v1/materials/{material}.hmat"
                     if material else " []")
        add(name, position, scale, f"""    MeshRendererComponent:
      Version: 1
      Mesh: {reference}
      Materials:{overrides}
""")

    add("Matched camera center pose", (0, 2.6, 7.5), (1, 1, 1), """    CameraComponent:
      Camera:
        ProjectionType: 0
        PerspectiveFOV: .872664626
        PerspectiveNear: .1
        PerspectiveFar: 50
        OrthographicSize: 10
        OrthographicNear: -1
        OrthographicFar: 1
      Primary: true
      FixedAspectRatio: false
""")
    entities[-1] = entities[-1].replace("Rotation: [0, 0, 0]", "Rotation: [-.195023, 0, 0]")
    mesh("Room floor", (0, -.11, 0), (11.2, .2, 6))
    mesh("Continuous reflective threshold", (0, .54, 2.22), (10, .13, .55), material="2_2")
    mesh("Back wall", (0, 1.55, -3), (11.2, 3.1, .15))
    mesh("Central wall and open threshold", (0, 1.55, -2.08), (.14, 3.1, 1.75))
    for name, sign, color in (("Warm", -1, (1, .58, .3)), ("Cool", 1, (.31, .62, 1))):
        mesh(f"{name} side wall", (sign * 5.57, 1.55, 0), (.15, 3.1, 6))
        for metal, roughness, z in ((True, False, .9), (True, True, -.4), (False, False, .9), (False, True, -.4)):
            mesh(f"{name} {'metal' if metal else 'dielectric'} {'rough' if roughness else 'smooth'}",
                 (sign * (2.25 if metal else 3.85), .62, z), (.98, .98, .98), True,
                 f"{2 if metal else 0}_{4 if roughness else 0}")
        add(f"{name} room light", (sign * 2.8, 2.8, .15), (1, 1, 1), f"""    PointLightComponent:
      Color: {list(color)}
      Intensity: 7
      Enabled: true
      Range: 5
""")
        add(f"{name} capture point", (sign * 2.55, 1.46, -.03), (1, 1, 1), f"""    ReflectionProbeComponent:
      Environment: ReflectionStudy/{name}.henv
      InfluenceHalfSize: [3.45, 1.52, 3]
      ProjectionOffset: [0, .04, 0]
      ProjectionHalfSize: [3.45, 1.56, 3]
      BlendDistance: 1.1
      Enabled: true
""")
    return header + "".join(entities)


def timing_summary(path, count=3):
    with path.open() as stream:
        rows = list(csv.DictReader(stream))
    summary = {}
    for width, height in ((1280, 720), (1920, 1080), (2560, 1440)):
        cases = [[float(row["forwardGPUms"]) for row in rows if
                  (int(row["width"]), int(row["height"]), int(row["probes"])) == (width, height, n)]
                 for n in range(count)]
        if any(len(case) != 15 for case in cases):
            raise RuntimeError("Incomplete forward timing sample set")
        summary[f"{width}x{height}"] = {
            "medianGPUms": [statistics.median(case) for case in cases],
            "p95GPUms": [sorted(case)[math.ceil(.95 * len(case)) - 1] for case in cases],
            "pairedDeltaMedianGPUms": [statistics.median(test - base for base, test in zip(cases[0], case))
                                       for case in cases[1:]],
        }
    return summary


def qualify_archive(archive, out, report, offscreen_only, diagnostic=False, ssr_gallery=None, hdr_bloom=None,
                    defocus_study=False, shutter_study=False, lens_study=False, grade_study=False):
    from internal.package_tests import extract_verified
    from macos.packaging import relocate_executable, sign
    from macos.qualification import isolation_command, product_acceptance

    sdk = hz.mono_prefix()
    report["relocation"] = {}
    with tempfile.TemporaryDirectory(prefix="Hazel Reflection Study café ") as temporary:
        working = Path(temporary)
        root = extract_verified(archive, working)
        bundle = root / "Nutella.app"
        contents = bundle / "Contents"
        resources = contents / "Resources"
        meta = json.loads((resources / "build.json").read_text())
        artifact = resources / "shaders/metal/ForwardPBR.hzmetal"
        if hashlib.sha256(artifact.read_bytes()).hexdigest() != meta["metal_shaders"][artifact.name]:
            raise RuntimeError("Packaged ForwardPBR artifact differs from its manifest")
        if ssr_gallery:
            reflection = resources / "shaders/metal/ScreenReflections.hzmetal"
            if hashlib.sha256(reflection.read_bytes()).hexdigest() != meta["metal_shaders"][reflection.name]:
                raise RuntimeError("Packaged SSR artifact differs from its manifest")
        if hdr_bloom:
            for name in ("BloomFilter.hzmetal", "WorldPresentation.hzmetal", "ExposureMeter.metallib",
                         "DefocusTile.hzmetal", "DefocusPrefilter.hzmetal", "DefocusGather.hzmetal",
                         "ShutterFilter.hzmetal", "LensFlare.hzmetal"):
                shader = resources / "shaders/metal" / name
                if hashlib.sha256(shader.read_bytes()).hexdigest() != meta["metal_shaders"][name]:
                    raise RuntimeError(f"Packaged {name} differs from its manifest")
        runtime_assets = resources / "Project"
        source_cubes = [p for p in runtime_assets.glob("**/CapturedReflections/**/*.hlight")
                        if not any(part.endswith(".henv.prepared") for part in p.parts)]
        if list(runtime_assets.rglob("*.hdr")) or source_cubes:
            raise RuntimeError("Runtime project archive retained HDR producer inputs")
        hz.run(["codesign", "--verify", "--deep", "--strict", bundle])
        report["relocation"]["archiveIntegrity"] = "PASS"

        source_free = working / "Source-free Metal Resources"
        shutil.copytree(resources, source_free, ignore=shutil.ignore_patterns("*.glsl", "*.metal"))
        for backend, binary, resource_root in (
            ("metal", "MetalRenderer2DQuadSmoke", source_free),
            ("opengl", "MigrationRenderer2DSmoke", resources),
        ):
            exe = working / "Probe/MacOS" / binary
            if not exe.parent.exists():
                exe.parent.mkdir(parents=True)
                shutil.copytree(contents / "Frameworks", exe.parent.parent / "Frameworks")
            shutil.copy2(hz.binaries("Release") / binary / binary, exe)
            relocate_executable(exe)
            sign(exe)
            filename = hdr_bloom or ssr_gallery or (gallery.LOCAL_PROJECT if diagnostic else PROJECT_NAME)
            project = resource_root / "Project" / filename
            variables = {key: value for key, value in os.environ.items()
                         if not key.startswith(("HAZEL_", "MTL_", "DYLD_"))}
            variables.update(PATH="/usr/bin:/bin", DEVELOPER_DIR="/nonexistent",
                             HAZEL_DATA=str(working / (backend + "-probe-profile")),
                             HAZEL_RESOURCES=str(resource_root), HAZEL_RENDERING_REFERENCE_PROJECT=str(project),
                             MTL_DEBUG_LAYER="1", MTL_SHADER_VALIDATION="1")
            if diagnostic:
                variables.update(HAZEL_REFLECTION_DIAGNOSTIC="1", HAZEL_REFLECTION_GALLERY_CENTER="1")
            if ssr_gallery:
                variables.update(HAZEL_SSR_GALLERY="1", HAZEL_REFLECTION_DIAGNOSTIC="1")
            if hdr_bloom:
                variables.update(HAZEL_HDR_BLOOM="1", HAZEL_REFLECTION_DIAGNOSTIC="1")
            if defocus_study:
                variables["HAZEL_CAMERA_DEFOCUS_TEST"] = "1"
            if shutter_study:
                variables["HAZEL_CAMERA_SHUTTER_TEST"] = "1"
            if lens_study:
                variables["HAZEL_CAMERA_LENS_TEST"] = "1"
            if grade_study:
                variables["HAZEL_CAMERA_GRADE_TEST"] = "1"
            log = out / f"{backend}-source-free.log"
            captures = working / f"{backend}-source-free-captures"
            command = [exe, resource_root, captures, "--reflection-probes"] if backend == "metal" else [
                       exe, "--reflection-probes", captures]
            with log.open("w") as stream:
                result = subprocess.run(isolation_command(command, sdk, metal=backend == "metal"), cwd=working,
                                        env=variables, stdout=stream, stderr=subprocess.STDOUT, timeout=120)
            report["relocation"][backend + "SourceFree"] = {"exitCode": result.returncode,
                "results": [line for line in log.read_text().splitlines()
                            if line.startswith(("REFLECTION_", "SSR_", "BLOOM_", "CAMERA_", "DEFOCUS_", "SHUTTER_", "LENS_", "GRADE_"))]}
            if result.returncode:
                raise RuntimeError(f"Source-free {backend} reflection failed. See {log}")
        if offscreen_only:
            return
        failures = []
        for backend in ("metal", "opengl"):
            run = working / backend
            run.mkdir()
            try:
                product_acceptance("Nutella", bundle, run, out, sdk, metal=backend == "metal")
                report["relocation"][backend + "Player"] = "PASS"
            except RuntimeError as error:
                report["relocation"][backend + "Player"] = str(error).splitlines()[0]
                failures.append(report["relocation"][backend + "Player"])
        if failures:
            raise RuntimeError("; ".join(failures))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", choices=("Debug", "Release"), required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--backend", choices=("both", "metal", "opengl"), default="both")
    parser.add_argument("--project", type=Path, help="Reuse a previously prepared study project")
    parser.add_argument("--prepare-only", action="store_true")
    parser.add_argument("--probe-side", type=int, choices=(128, 256, 512))
    parser.add_argument("--panorama-width", type=int, choices=(512, 1024, 2048))
    parser.add_argument("--center-only", action="store_true", help="Capture only the Gallery center pose")
    parser.add_argument("--diagnostic", action="store_true", help="Generate and capture the visual reflection gallery")
    parser.add_argument("--resources", type=Path, default=RESOURCES)
    parser.add_argument("--working-directory", type=Path, default=hz.ROOT)
    parser.add_argument("--benchmark", action="store_true")
    parser.add_argument("--archive", type=Path, help="Qualify an existing source-free Nutella Release archive")
    parser.add_argument("--offscreen-only", action="store_true", help="Use isolated archive render/readback without desktop smoke")
    parser.add_argument("--baseline", help="Benchmark the zero-probe path of this Git revision too")
    args = parser.parse_args()
    if (args.probe_side or args.panorama_width or args.center_only) and not args.diagnostic:
        parser.error("Gallery options require --diagnostic")
    if args.project and (args.probe_side or args.panorama_width):
        parser.error("Source options apply when generating a new Gallery project")
    if args.diagnostic and args.benchmark:
        parser.error("--diagnostic selects the visual study")
    if args.offscreen_only and not args.archive:
        parser.error("--offscreen-only requires --archive")
    if args.archive and (args.config != "Release" or args.project or args.benchmark or args.prepare_only):
        parser.error("--archive requires Release and runs independently of the study and benchmark")
    if args.baseline and (not args.benchmark or args.backend != "metal"):
        parser.error("--baseline requires --benchmark --backend metal")
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    if any(out.iterdir()):
        parser.error("Use a fresh evidence directory")
    resources = args.resources.resolve()
    working = args.working_directory.resolve()
    env = {key: value for key, value in os.environ.items() if not key.startswith(("HAZEL_", "MTL_", "DYLD_"))}
    env.update(DYLD_LIBRARY_PATH=str(hz.mono_prefix() / "lib"), HAZEL_RESOURCES=str(resources))
    report = {"config": args.config, "head": hz.output(["git", "rev-parse", "HEAD"]),
              "artifactSHA256": hashlib.sha256((resources / "shaders/metal/ForwardPBR.hzmetal").read_bytes()).hexdigest(),
              "status": "INCOMPLETE"}
    source = resources / "shaders/ForwardPBR.glsl"
    if source.is_file():
        report["shaderSHA256"] = hashlib.sha256(source.read_bytes()).hexdigest()
    elif args.backend != "metal":
        parser.error("OpenGL requires canonical shader source")
    try:
        if args.archive:
            qualify_archive(args.archive.resolve(), out, report, args.offscreen_only, args.diagnostic)
            report["status"] = "PASS native reflection archive relocation"
            return
        if args.project:
            project = args.project.resolve()
        else:
            report["bakeWallSeconds"] = prepare(out / "Study Project", args.config, env, args.diagnostic,
                                                      args.probe_side or 256, args.panorama_width or 512)
            project = out / "Study Project" / (gallery.LOCAL_PROJECT if args.diagnostic else PROJECT_NAME)
        report["project"] = str(project)
        if args.prepare_only:
            report["status"] = "PREPARED"
            return
        env["HAZEL_RENDERING_REFERENCE_PROJECT"] = str(project)
        if args.diagnostic:
            env["HAZEL_REFLECTION_DIAGNOSTIC"] = "1"
        if args.center_only:
            env["HAZEL_REFLECTION_GALLERY_CENTER"] = "1"
        pose_count = (1 if args.center_only else gallery.POSES) if args.diagnostic else 9
        if args.benchmark:
            env["HAZEL_REFLECTION_BENCHMARK"] = "1"
        else:
            env.update(MTL_DEBUG_LAYER="1", MTL_SHADER_VALIDATION="1")
        report["backends"] = {}
        for backend in ("metal", "opengl") if args.backend == "both" else (args.backend,):
            binary = "MetalRenderer2DQuadSmoke" if backend == "metal" else "MigrationRenderer2DSmoke"
            folder = out / backend
            arguments = [resources, folder, "--reflection-probes"] if backend == "metal" else ["--reflection-probes", folder]
            command = [hz.binaries(args.config) / binary / binary, *arguments]
            env["HAZEL_DATA"] = str(out / f"{backend}-profile")
            with (out / f"{backend}.log").open("w") as log:
                result = subprocess.run(command, env=env, cwd=working, stdout=log, stderr=subprocess.STDOUT, timeout=360)
            log_lines = (out / f"{backend}.log").read_text().splitlines()
            report["backends"][backend] = {
                "command": list(map(str, command)), "exitCode": result.returncode,
                "results": [line for line in log_lines if line.startswith("REFLECTION_")],
            }
            if result.returncode:
                raise RuntimeError(f"{backend} failed. See {out / (backend + '.log')}")
            if args.benchmark:
                report.setdefault("timing", {})[backend] = timing_summary(folder / "timing.csv")
                if args.baseline:
                    baseline = out / "baseline-resources"
                    shutil.copytree(resources, baseline)
                    from macos.metal_shaders import produce

                    relative_source = "shaders/ForwardPBR.glsl"
                    checkpoint = subprocess.check_output(
                        ["git", "show", f"{args.baseline}:Hazel/Resources/{relative_source}"],
                        cwd=hz.ROOT, text=True)
                    old_layout = "vec4 u_EnvRotationGain;vec4 u_EnvOptions;};"
                    if checkpoint.count(old_layout) != 1:
                        raise RuntimeError("Baseline must use the checkpoint environment block")
                    # Share only the current ABI with the historical shader. Its
                    # executable shading path is the exact checkpoint source.
                    checkpoint = checkpoint.replace(old_layout,
                        "vec4 u_EnvRotationGain;vec4 u_EnvOptions;vec4 u_ReflectionProbes[8];};")
                    (baseline / relative_source).write_text(checkpoint)
                    produce("Release", baseline / "shaders/metal", sources=baseline / "shaders")
                    baseline_env = {**env, "HAZEL_RESOURCES": str(baseline),
                                    "HAZEL_REFLECTION_BENCHMARK": "global-only"}
                    with (out / "baseline.log").open("w") as log:
                        subprocess.run([command[0], baseline, out / "baseline", "--reflection-probes"],
                                       env=baseline_env, cwd=working, stdout=log, stderr=subprocess.STDOUT,
                                       check=True, timeout=360)
                    report["baseline"] = {"revision": hz.output(["git", "rev-parse", args.baseline]),
                                          "abi": "checkpoint shading source with matching environment UBO padding",
                                          "timing": timing_summary(out / "baseline/timing.csv", 1)}
            else:
                selected_poses = {0, pose_count // 2, pose_count - 1}
                for capture in folder.glob("*.bin"):
                    if int(capture.stem.split("-")[-1]) in selected_poses:
                        png(capture.with_suffix(".png"), 640, 360, pixels(capture))
                if args.diagnostic:
                    center = pose_count // 2
                    local = pixels(folder / f"local-{center}.bin")
                    changes = {}
                    for name in ("global", "distant"):
                        other = pixels(folder / f"{name}-{center}.bin")
                        changes[name] = sum(max(abs(a[c] - b[c]) for c in range(3)) > .08
                                            for a, b in zip(local, other) if a[4] == b[4] and a[4] != -1)
                        if changes[name] < 1200:
                            raise RuntimeError(f"{backend} gallery {name} control lacks visible separation")
                    report["backends"][backend]["centerPixelsChangedAbove008"] = changes
        if args.backend == "both" and not args.benchmark:
            report["visualParity"] = {}
            cases = [(mode, pose) for mode in ("global", "local") for pose in range(pose_count)]
            if args.diagnostic:
                cases.append(("distant", pose_count // 2))
            for mode, pose in cases:
                name = f"{mode}-{pose}"
                a = pixels(out / "metal" / f"{name}.bin")
                b = pixels(out / "opengl" / f"{name}.bin")
                if len(a) != len(b) or len(a) != 640 * 360:
                    raise RuntimeError("Incomplete matched image")
                stable = [max(abs(x[c] - y[c]) for c in range(3))
                          for x, y in zip(a, b) if x[4] == y[4] and x[4] != -1]
                difference = sorted(stable)
                p99 = difference[int(.99 * len(difference))]
                mismatched_ids = sum(x[4] != y[4] for x, y in zip(a, b))
                report["visualParity"][name] = {"sameSurfaceRGBp99": p99, "sameSurfaceRGBMax": difference[-1],
                                                 "idDifferences": mismatched_ids}
                if p99 > .015 or mismatched_ids > 460:
                    raise RuntimeError(f"Reflection study parity exceeded bounds in {name}")
        if args.diagnostic and not args.center_only:
            media_backend = "opengl" if args.backend == "opengl" else "metal"
            gallery.write_motion(out / "matched-motion.png", out / media_backend, pixels)
        report["status"] = "PASS focused native reflection probes"
    except Exception as error:
        report["error"] = str(error)
        raise
    finally:
        (out / "results.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
