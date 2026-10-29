"""Xcode managed build phase; uses only the checkout-owned pinned Mono SDK."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import hazel

configuration, name = sys.argv[1:]
sdk = hazel.mono_prefix()
destination = hazel.binaries(configuration) / name / (name + ('.dll' if name == 'Hazel-ScriptCore' else '.exe'))
destination.parent.mkdir(parents=True, exist_ok=True)
sources = sorted((hazel.ROOT / 'Hazel-ScriptCore').rglob('*.cs')) if name == 'Hazel-ScriptCore' else [hazel.ROOT / 'scripts/internal/managed/PackageAudit.cs']
hazel.run([*hazel.mono_command(sdk), '-sdk:4.7.2', '-target:library' if name == 'Hazel-ScriptCore' else '-target:exe', '-r:System.Core', '-out:' + str(destination), *sources])
