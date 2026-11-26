#!/usr/bin/env python3
"""Capture a Reflection Gallery project, prepare the FP32 cubes and compare readbacks."""
import argparse
from array import array
import json
import math
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import hazel as hz
from macos import reflection_diagnostic as gallery


def read_cube(path):
    data = path.read_bytes()
    assert data.startswith(b"HZLIGHT1")
    rgb = array("f")
    rgb.frombytes(data[72:])
    if sys.byteorder != "little":
        rgb.byteswap()
    return rgb


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", choices=("Debug", "Release"), required=True)
    parser.add_argument("--gallery", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--side", type=int, choices=(128, 256, 512), default=256)
    parser.add_argument("--backend", choices=("metal", "opengl", "both"), default="both")
    parser.add_argument("--full-motion", action="store_true")
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True)
    imported = args.gallery.resolve()
    for name in ("Warm", "Cool"):
        descriptor = imported.parent / "Assets/ReflectionStudy" / f"{name}.henv"
        if not re.search(rf"^    Specular: {args.side}$", descriptor.read_text(), re.MULTILINE):
            parser.error("The imported Gallery specular resolution must match --side")
    assets = hz.ROOT / "Hazel/Resources"
    env = {k: v for k, v in os.environ.items() if not k.startswith(("HAZEL_", "MTL_", "DYLD_"))}
    env.update(DYLD_LIBRARY_PATH=str(hz.mono_prefix() / "lib"), HAZEL_RESOURCES=str(assets),
               MTL_DEBUG_LAYER="1", MTL_SHADER_VALIDATION="1")
    report = {"configuration": args.config, "side": args.side, "status": "INCOMPLETE", "capture": {}}
    backends = ("metal", "opengl") if args.backend == "both" else (args.backend,)
    try:
        for backend in backends:
            folder = out / backend
            binary = "MetalRenderer2DQuadSmoke" if backend == "metal" else "MigrationRenderer2DSmoke"
            arguments = [assets, folder, "--reflection-probes"] if backend == "metal" else ["--reflection-probes", folder]
            command = [hz.binaries(args.config) / binary / binary, *arguments]
            with (out / f"{backend}.log").open("w") as log:
                subprocess.run(command, check=True, stdout=log, stderr=subprocess.STDOUT, cwd=hz.ROOT, timeout=120,
                               env={**env, "HAZEL_DATA": str(out / (backend + "-profile")),
                                    "HAZEL_RENDERING_REFERENCE_PROJECT": str(imported),
                                    "HAZEL_REFLECTION_DIAGNOSTIC": "1", "HAZEL_PROBE_CAPTURE_SIDE": str(args.side),
                                    "HAZEL_PROBE_AXIS_ROOT": str(hz.ROOT / "examples/RenderingReference/Assets")})
            report["capture"][backend] = [line for line in (out / f"{backend}.log").read_text().splitlines()
                                          if line.startswith("REFLECTION_CAPTURE")]
        if len(backends) == 2:
            report["sourceParity"] = {}
            for name in ("Warm", "Cool"):
                a, b = (read_cube(out / backend / f"{name}.hlight") for backend in backends)
                if len(a) != len(b) or len(a) != args.side**2 * 18:
                    raise RuntimeError("Incomplete native radiance captures")
                errors = sorted(abs(x - y) for x, y in zip(a, b))
                p99 = errors[int(.99 * len(errors))]
                report["sourceParity"][name] = dict(p99=p99, maximum=errors[-1], peakHDR=max(a))
                if p99 > .01 or not math.isfinite(max(a)):
                    raise RuntimeError(f"Captured {name} FP32 radiance differs between backends")

        project = out / "Captured Gallery"
        shutil.copytree(imported.parent, project, ignore=shutil.ignore_patterns("*.lock", "*.log"))
        baked = project / imported.name
        asset_root = project / "Assets"
        materialized = asset_root / "CapturedReflections"
        materialized.mkdir()
        report["preparation"] = {}
        for name in ("Warm", "Cool"):
            raw = out / backends[0] / f"{name}.hlight"
            shutil.copy2(raw, materialized / raw.name)
            reference = f"CapturedReflections/{name}.henv"
            (asset_root / reference).write_text(f"""Environment:
  Version: 1
  Name: Captured {name} gallery
  Notice: Procedural Hazel Gallery, Apache-2.0
  Source:
    Kind: SceneCapture
    Image: CapturedReflections/{name}.hlight
    Interpretation: LinearRec709D65Cube
  Preparation:
    Recipe: Hazel.IBL_v1.GGX-correlated-single.r045.E-pi.AB-NoV-r.cube-top
    Background: 16
    Specular: {args.side}
    Irradiance: {32 if args.side == 128 else 64}
    Samples: 1024
""")
            tool = hz.binaries(args.config) / "HazelProject/HazelProject"
            with (out / f"prepare-{name}.log").open("w") as log:
                subprocess.run([tool, "prepare-environment", baked, reference], check=True, env=env,
                               stdout=log, stderr=subprocess.STDOUT, timeout=300)
            log = (out / f"prepare-{name}.log").read_text()
            report["preparation"][name] = re.search(r"^Bake .*$", log, re.MULTILINE).group()
            accepted = (asset_root / f"{reference}.accepted").read_text()
            specular = re.search(r"^\s*Path: (.*?/Specular\.hlight)$", accepted, re.MULTILINE).group(1)
            pixels = raw.read_bytes()[72:]
            if (asset_root / specular).read_bytes()[72:72 + len(pixels)] != pixels:
                raise RuntimeError(f"{name} capture did not retain exact mip zero")

        warm = materialized / "Warm.hlight"
        accepted = materialized / "Warm.henv.accepted"
        original = accepted.read_bytes()
        backup = materialized / "Warm.source-copy"
        warm.rename(backup)
        try:
            warm.write_bytes(b"invalid capture")
            with (out / "prepare-invalid.log").open("w") as log:
                failed = subprocess.run([tool, "prepare-environment", baked, "CapturedReflections/Warm.henv"],
                                        env=env, stdout=log, stderr=subprocess.STDOUT, timeout=30)
            if failed.returncode == 0 or accepted.read_bytes() != original:
                raise RuntimeError("Failed preparation replaced the last accepted bake")
        finally:
            backup.replace(warm)
        report["failedPreparationRetention"] = "PASS"

        for variant in (gallery.LOCAL_SCENE, gallery.GLOBAL_SCENE):
            path = asset_root / variant
            text = path.read_text().replace("Environment: ReflectionStudy/", "Environment: CapturedReflections/")
            text = re.sub(r"(      Environment: CapturedReflections/[^\n]+\n)",
                          rf"\g<1>      BakeResolution: {args.side}\n", text)
            path.write_text(text)
        for name, descriptor in (("imported", imported), ("baked", baked)):
            command = [sys.executable, hz.ROOT / "scripts/macos/reflection_probes.py", "--config", args.config,
                       "--backend", args.backend, "--diagnostic", "--project", descriptor, "--output", out / name]
            if not args.full_motion:
                command.append("--center-only")
            with (out / f"{name}.log").open("w") as log:
                subprocess.run(command, check=True, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=360)
        report["status"] = "PASS focused capture and cooked Gallery comparison"
    finally:
        (out / "results.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
