#!/usr/bin/env python3
"""Stage and audit the local native Mono feasibility SDK; never installs it."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[2]
PIN = json.loads((ROOT / 'scripts/dependencies/mono.json').read_text())['macos_feasibility']
DEFAULT_ARCHIVE = ROOT / 'build/downloads/homebrew/downloads/f284772d4a167a61004fbaa10d579b7ad42f57d4c3d53baa74845b4c07000d92--mono--6.14.1.arm64_golden_gate.bottle.3.tar.gz'
DEFAULT_DESTINATION = ROOT / 'build/dependencies/mono/macos-arm64/sdk'


def output(*args):
    return subprocess.check_output(list(map(str, args)), text=True).strip()


def run(*args):
    print('+', ' '.join(map(str, args)), flush=True)
    subprocess.run(list(map(str, args)), check=True)


def digest(path):
    value = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''): value.update(block)
    return value.hexdigest()


def macho(path):
    return 'Mach-O' in output('file', path)


def minimum_version(path):
    lines = output('otool', '-l', path).splitlines()
    for index, line in enumerate(lines):
        if line.strip() == 'cmd LC_BUILD_VERSION':
            for candidate in lines[index:index + 10]:
                words = candidate.split()
                if len(words) == 2 and words[0] == 'minos': return words[1]
    return None


def dependencies(path):
    return [line.strip().split(' (', 1)[0] for line in output('otool', '-L', path).splitlines()[1:]]


def prepare(archive, destination):
    if digest(archive) != PIN['bottle_sha256']:
        raise RuntimeError('Mono candidate bottle checksum mismatch')
    if destination.exists():
        raise RuntimeError('Preserve existing local Mono SDK candidate: ' + str(destination))
    temporary_root = ROOT / 'build/dependencies/mono/macos-arm64'
    temporary_root.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='mono-extract-', dir=temporary_root) as temporary:
        with tarfile.open(archive) as package: package.extractall(temporary, filter='data')
        source = Path(temporary) / 'mono' / PIN['version']
        if not source.is_dir(): raise RuntimeError('Unexpected Mono bottle layout')
        shutil.copytree(source, destination, symlinks=True)

    required = ['bin/mono', 'include/mono-2.0/mono/jit/jit.h', 'lib/libmonosgen-2.0.dylib',
                'lib/mono/4.5/mcs.exe', 'lib/mono/4.5/mscorlib.dll',
                'lib/mono/4.7.2-api/mscorlib.dll', 'etc/mono/config', 'LICENSE', 'COPYING.LIB']
    missing = [name for name in required if not (destination / name).exists()]
    if missing: raise RuntimeError('Incomplete Mono candidate: ' + ', '.join(missing))

    native = []
    for path in sorted(destination.rglob('*')):
        if not path.is_file() or path.is_symlink(): continue
        if path.suffix != '.dylib' and not os.access(path, os.X_OK): continue
        if not macho(path): continue
        path.chmod(path.stat().st_mode | stat.S_IWUSR)
        original = dependencies(path)
        for dependency in original:
            marker = '@@HOMEBREW_PREFIX@@/opt/mono/lib/'
            if dependency.startswith(marker):
                run('install_name_tool', '-change', dependency, '@rpath/' + Path(dependency).name, path)
        if path.suffix == '.dylib':
            run('install_name_tool', '-id', '@rpath/' + path.name, path)
        run('codesign', '--force', '--sign', '-', path)
        native.append({
            'path': path.relative_to(destination).as_posix(),
            'sha256': digest(path),
            'architectures': output('lipo', '-archs', path).split(),
            'minimum_macos': minimum_version(path),
            'dependencies': dependencies(path),
        })

    bad_architecture = [item['path'] for item in native if item['architectures'] != ['arm64']]
    bad_dependency = [(item['path'], dependency) for item in native for dependency in item['dependencies']
                      if '@@HOMEBREW' in dependency or '/opt/homebrew' in dependency or '/Cellar/' in dependency]
    if bad_architecture: raise RuntimeError('Non-arm64 Mono native files: ' + ', '.join(bad_architecture))
    if bad_dependency: raise RuntimeError('Unrelocated Mono dependency: ' + repr(bad_dependency))

    required_hashes = {name: digest((destination / name).resolve()) for name in required}
    manifest = {
        'status': PIN['status'],
        'source': {'version': PIN['version'], 'url': PIN['source_url'], 'sha256': PIN['source_sha256']},
        'producer': {
            'formula_revision': PIN['formula_revision'], 'bottle_tag': PIN['bottle_tag'],
            'bottle_url': PIN['bottle_url'], 'bottle_sha256': PIN['bottle_sha256'],
            'bottle_archive': archive.name,
            'requirements': ['Homebrew is evidence/acquisition tooling for this local candidate only',
                             'A production macOS 13 SDK must be rebuilt from the pinned source with a proven arm64 producer toolchain'],
        },
        'consumer': {
            'homebrew_required': False,
            'observed_minimum_macos': max((item['minimum_macos'] for item in native if item['minimum_macos']), default=None),
            'configuration': 'Pass explicit lib and etc roots to mono_set_dirs and mono_config_parse',
        },
        'license': PIN['license'],
        'required_files': required_hashes,
        'native_files': native,
    }
    manifest_path = destination.parent / 'candidate-manifest.json'
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n')
    print('Staged local Mono feasibility SDK:', destination)
    print('Manifest:', manifest_path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', type=Path, default=DEFAULT_ARCHIVE)
    parser.add_argument('--destination', type=Path, default=DEFAULT_DESTINATION)
    args = parser.parse_args()
    prepare(args.archive.resolve(), args.destination.resolve())


if __name__ == '__main__': main()
