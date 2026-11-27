#!/usr/bin/env python3
"""Author and qualify the native screen-space reflection gallery."""
import argparse
import csv
import hashlib
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
from macos import reflection_diagnostic as gallery
from macos.reflection_probes import RESOURCES, prepare, qualify_archive
from macos.xegtao import pixels, png

PROJECT = "SSR Gallery.hproj"
SCENE = "Scenes/SSRGallery.hazel"
WIDTH, HEIGHT = 640, 360
POSES = 17


def write_gallery(root, config, env, prepared):
    if prepared:
        shutil.copytree(prepared.parent / "Assets", root / "Assets")
        project = prepared.read_text()
    else:
        prepare(root, config, env, diagnostic=True, probe_side=128)
        project = (root / gallery.LOCAL_PROJECT).read_text()
    assets = root / "Assets"
    scene_file = re.search(r"(?m)^  StartScene: (.+)$", project)[1]
    scene = (assets / scene_file).read_text().replace("Scene: Reflection Gallery", "Scene: SSR Gallery", 1)
    scene = scene.replace("  Environment:", """  ScreenReflections:
    Enabled: true
    Quality: 1
    Steps: 64
    Distance: 20
    Thickness: .15
  Environment:""", 1)
    materials = assets / "ReflectionStudy/Materials"
    mirror = (materials / "Mirror strip.hmat").read_text()
    for name, rough in (("Polished floor", .055), ("Satin floor", .28), ("Matte floor", .61)):
        text = re.sub(r"(?m)^  Name: .*", f"  Name: {name}", mirror)
        text = re.sub(r"(?m)^  Roughness: .*", f"  Roughness: {rough}", text)
        (materials / f"{name}.hmat").write_text(text)
    # The left, center and right floor regions expose roughness under the same
    # lighting, while the rail and two rooms stress probe and screen boundaries.
    floor_start = scene.index("      Tag: Floor\n")
    floor_end = scene.index("  - Entity:", floor_start)
    scene = scene[:floor_start] + scene[floor_start:floor_end].replace(
        "      Translation: [0, -0.1, 0.2]", "      Translation: [-4.365, -0.1, 0.2]").replace(
        "      Scale: [13.1, 0.2, 10]", "      Scale: [4.37, 0.2, 10]").replace(
        "Materials/Graphite.hmat", "Materials/Polished floor.hmat") + scene[floor_end:]
    for name, translation, scale in (("Shared mirror rail", [0, .045, 1.65], [11.9, .06, 1.48]),
                                      ("Mirror rail base", [0, .01, 1.65], [12, .02, 1.57])):
        pattern = rf"(      Tag: {name}\n    TransformComponent:\n)      Translation: [^\n]+\n(      Rotation: [^\n]+\n)      Scale: [^\n]+"
        scene, count = re.subn(pattern, rf"\g<1>      Translation: {translation}\n\g<2>      Scale: {scale}", scene)
        if count != 1:
            raise RuntimeError(f"Missing Gallery geometry: {name}")
    extras = (("Satin floor", [0, -.1, .2], [4.36, .2, 10], "Satin floor"),
              ("Matte floor", [4.365, -.1, .2], [4.37, .2, 10], "Matte floor"),
              ("Thin warm upright", [-3.1, .8, 3.1], [.08, 1.6, .12], "Warm panel 3"),
              ("Thin cool upright", [1.4, .7, 3.4], [.07, 1.4, .10], "Cool panel 3"))
    order = len(re.findall(r"(?m)^  - Entity:", scene))
    for i, (name, position, scale, material) in enumerate(extras):
        scene += f"""  - Entity: {1000+i}
    Relationship:
      Parent: 0
      Order: {order+i}
    TagComponent:
      Tag: {name}
    TransformComponent:
      Translation: {position}
      Rotation: [0, 0, 0]
      Scale: {scale}
    MeshRendererComponent:
      Version: 1
      Mesh: Native/Direct_v1/UnitCube.hmesh
      Materials:
        - Slot: '0000000000000007'
          Material: ReflectionStudy/Materials/{material}.hmat
"""
    (assets / SCENE).write_text(scene)
    project = project.replace(scene_file, SCENE)
    project = re.sub(r"(?m)^  Name: .*", "  Name: SSR Gallery", project)
    result = root / PROJECT
    result.write_text(project)
    return result


def native_command(config, backend, resources, captures):
    if backend == "metal":
        return [hz.binaries(config) / "MetalRenderer2DQuadSmoke/MetalRenderer2DQuadSmoke",
                resources, captures, "--reflection-probes"]
    return [hz.binaries(config) / "MigrationRenderer2DSmoke/MigrationRenderer2DSmoke",
            "--reflection-probes", captures]


def timing(log, backend, raw):
    feedback = {}
    for serial, start, end in re.findall(r"OFFSCREEN_FEEDBACK frame=(\d+) gpu_start=([\d.]+) gpu_end=([\d.]+)", log):
        feedback[int(serial)] = (float(end) - float(start)) * 1000
    samples = {}
    for line in log.splitlines():
        if not line.startswith("SSR_TIMING "):
            continue
        w, h, enabled, sample, serial, ns = map(int, line.split()[1:])
        ms = feedback[serial] if backend == "metal" else ns / 1e6
        if ms <= 0:
            raise RuntimeError("Missing native GPU elapsed interval")
        samples.setdefault(f"{w}x{h}", [{}, {}])[enabled][sample] = ms
    if len(samples) != 3 or any(len(state) != 15 for states in samples.values() for state in states):
        raise RuntimeError("Incomplete timing samples")
    with raw.open("w", newline="") as stream:
        rows = csv.writer(stream, lineterminator="\n")
        rows.writerow(("format", "sample", "offGPUms", "onGPUms"))
        for extent, (off, on) in samples.items():
            rows.writerows((extent, i, off[i], on[i]) for i in sorted(off))
    def summarize(values):
        values = sorted(values)
        return {"median": statistics.median(values), "p95": values[14]}
    return {name: {"off": summarize(off.values()), "on": summarize(on.values()),
                   "pairedDelta": summarize([on[i] - off[i] for i in off])}
            for name, (off, on) in samples.items()}


def compare(out, report):
    report["parity"] = {}
    for pose in range(POSES):
        a, b = (pixels(out / backend / f"on-{pose}.bin") for backend in ("metal", "opengl"))
        dif = sorted(max(abs(x[c] - y[c]) for c in range(3))
                     for x, y in zip(a, b) if x[4] == y[4] and x[4] != -1)
        ids = sum(x[4] != y[4] for x, y in zip(a, b))
        report["parity"][pose] = dict(stableRGBp99=dif[int(.99 * len(dif))], idDifferences=ids)
    if max(v["stableRGBp99"] for v in report["parity"].values()) > .025 or max(
            v["idDifferences"] for v in report["parity"].values()) > 460:
        raise RuntimeError("SSR exceeded native parity bounds")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--config", choices=("Release", "Debug"), default="Release")
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--from-gallery", type=Path, help="Reuse a prepared local or baked Reflection Gallery")
    p.add_argument("--project", type=Path, help="Use an already-authored SSR Gallery")
    p.add_argument("--backend", choices=("both", "metal", "opengl"), default="both")
    p.add_argument("--resources", type=Path, default=RESOURCES)
    p.add_argument("--benchmark", action="store_true")
    p.add_argument("--motion", action="store_true", help="Encode paired probe-only and SSR camera-motion APNG")
    p.add_argument("--prepare-only", action="store_true")
    p.add_argument("--archive", type=Path, help="Qualify a Nutella SSR Gallery archive")
    p.add_argument("--offscreen-only", action="store_true", help="Skip native packaged player presentation")

    args = p.parse_args()
    if args.archive and (args.config != "Release" or args.project or args.benchmark or args.prepare_only):
        p.error("--archive runs independently with Release")
    if args.offscreen_only and not args.archive:
        p.error("--offscreen-only requires --archive")
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    if any(out.iterdir()):
        p.error("Use a fresh evidence directory")
    env = {k: v for k, v in os.environ.items() if not k.startswith(("HAZEL_", "MTL_", "DYLD_"))}
    env.update(HAZEL_RESOURCES=str(args.resources.resolve()), DYLD_LIBRARY_PATH=str(hz.mono_prefix() / "lib"))
    report = {"config": args.config, "status": "INCOMPLETE",
              "head": hz.output(["git", "rev-parse", "HEAD"])}
    shader = args.resources / "shaders/ScreenReflections.glsl"
    if not args.archive and shader.is_file():
        report["resolveSHA256"] = hashlib.sha256(shader.read_bytes()).hexdigest()
    try:
        if args.archive:
            qualify_archive(args.archive.resolve(), out, report, args.offscreen_only, ssr_gallery=PROJECT)
            report["status"] = "PASS native SSR archive relocation"
            return
        project = args.project.resolve() if args.project else write_gallery(
            out / "SSR Gallery", args.config, env, args.from_gallery.resolve() if args.from_gallery else None)
        report["project"] = str(project)
        if args.prepare_only:
            report["status"] = "PREPARED"
            return
        env.update(HAZEL_RENDERING_REFERENCE_PROJECT=str(project), HAZEL_SSR_GALLERY="1", HAZEL_REFLECTION_DIAGNOSTIC="1")
        if args.benchmark:
            env.update(HAZEL_SSR_BENCHMARK="1", HAZEL_METAL_TRACE="1")
        else:
            env.update(MTL_DEBUG_LAYER="1", MTL_SHADER_VALIDATION="1")
        backends = ("metal", "opengl") if args.backend == "both" else (args.backend,)
        report["backends"] = {}
        for backend in backends:
            folder = out / backend
            cmd = native_command(args.config, backend, args.resources.resolve(), folder)
            log_path = out / f"{backend}.log"
            with log_path.open("w") as log:
                result = subprocess.run(cmd, cwd=hz.ROOT, env={**env, "HAZEL_DATA": str(out / f"{backend}-profile")},
                                        stdout=log, stderr=subprocess.STDOUT, timeout=360)
            log = log_path.read_text()
            report["backends"][backend] = {"command": list(map(str, cmd)), "exitCode": result.returncode,
                                           "checks": [line for line in log.splitlines()
                                                      if line.startswith(("SSR_ORACLE ", "SSR_COMPOSITE ",
                                                                          "SSR_LIFETIME ", "SSR_DISOCCLUSION ", "SSR_EDGE ", "SSR_GALLERY "))]}
            if result.returncode:
                raise RuntimeError(f"{backend} failed. See {log_path}")
            if args.benchmark:
                report["backends"][backend]["gpuMs"] = timing(log, backend, out / f"{backend}-timing.csv")
            else:
                for mode in ("on", "off", "global", "temporal"):
                    for pose in (0, POSES // 2, POSES - 1):
                        capture = folder / f"{mode}-{pose}.bin"
                        if capture.is_file():
                            png(capture.with_suffix(".png"), WIDTH, HEIGHT, pixels(capture))
        if len(backends) == 2 and not args.benchmark:
            compare(out, report)
        if args.motion and not args.benchmark:
            gallery.write_motion(out / "probe-and-ssr-motion.png", out / backends[0], pixels,
                                 modes=("off", "on"), poses=POSES)
        report["status"] = "PASS focused native " + ("GPU timing" if args.benchmark else "SSR gallery")
    except Exception as error:
        report["error"] = str(error)
        raise
    finally:
        (out / "results.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
