"""Xcode managed build phase; uses only the checkout-owned pinned Mono SDK."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import hazel

configuration, name = sys.argv[1:]
sdk = hazel.mono_prefix()
library = name != 'PackageAudit'
destination = hazel.binaries(configuration) / name / (name + ('.dll' if library else '.exe'))
destination.parent.mkdir(parents=True, exist_ok=True)
references = ['-r:System.Core']
if name == 'Hazel-ScriptCore': sources = sorted((hazel.ROOT / 'Hazel-ScriptCore').rglob('*.cs'))
elif name == 'MigrationManagedFixture':
    sources = [hazel.ROOT/'tests/migration/ManagedFixture.cs', *sorted((hazel.ROOT/'tests/fixtures/AuthoringProject/Assets/Scripts/Source').rglob('*.cs'))]
    references.append('-r:'+str(hazel.binaries(configuration)/'Hazel-ScriptCore/Hazel-ScriptCore.dll'))
else: sources = [hazel.ROOT / 'scripts/internal/managed/PackageAudit.cs']
hazel.run([*hazel.mono_command(sdk), '-sdk:4.7.2', '-target:library' if library else '-target:exe', *references, '-out:' + str(destination), *sources])
