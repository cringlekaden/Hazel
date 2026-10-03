#!/usr/bin/env python3
"""Build pinned shader tools without modifying vendor source or system packages."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import os
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
PINS_FILE = Path(__file__).with_name('shader-tools.json')
PINS = json.loads(PINS_FILE.read_text())


def run(*args, cwd=None, env=None):
    print('+', ' '.join(map(str, args)), flush=True)
    subprocess.run(list(map(str, args)), cwd=cwd, check=True, env=env)


def output(*args, cwd=None):
    return subprocess.check_output(list(map(str, args)), cwd=cwd, text=True).strip()


def checkout(name, destination):
    pin = PINS[name]
    if not destination.exists():
        destination.mkdir(parents=True)
        run('git', 'init', destination)
        run('git', '-C', destination, 'remote', 'add', 'origin', pin['url'])
    if output('git', '-C', destination, 'status', '--porcelain'):
        raise RuntimeError(f'Preserve modified dependency checkout: {destination}')
    if output('git', '-C', destination, 'remote', 'get-url', 'origin') != pin['url']:
        raise RuntimeError(f'Unexpected dependency remote: {destination}')
    current = subprocess.run(['git', '-C', str(destination), 'rev-parse', 'HEAD'],
                             text=True, capture_output=True)
    if current.returncode or current.stdout.strip() != pin['revision']:
        run('git', '-C', destination, 'fetch', '--depth=1', 'origin', pin['revision'])
        run('git', '-C', destination, 'checkout', '--detach', pin['revision'])
    if output('git', '-C', destination, 'rev-parse', 'HEAD') != pin['revision']:
        raise RuntimeError(f'Dependency revision mismatch: {name}')


def build(configuration, options, sources, system):
    prefix = ROOT / 'build/dependencies/install' / f'{configuration}-{system}-x86_64'
    libdir = prefix / 'lib'
    libdir.mkdir(parents=True, exist_ok=True)
    include = prefix / 'include'
    include.mkdir(parents=True, exist_ok=True)
    workspace = ROOT / 'build/dependencies/premake'
    if system == 'windows':
        run(options.msbuild, workspace / 'HazelShaderTools.sln', f'/m:{options.jobs}',
            f'/p:Configuration={configuration}', '/p:Platform=x64', '/verbosity:minimal')
    else:
        make_arguments = ['make', '-C', workspace, f'config={configuration.lower()}', f'-j{options.jobs}']
        # GNU 4.4's FIFO jobserver failed on this host; older Make uses pipes
        # already and does not understand --jobserver-style.
        version = output('make', '--version').splitlines()[0].split()[-1]
        import re
        numbers = tuple(map(int, re.findall(r'\d+', version)[:2]))
        if numbers >= (4, 4):
            make_arguments.append('--jobserver-style=pipe')
        run(*make_arguments)
    artifacts = workspace / 'bin' / f'{configuration}-{system}-x86_64'
    library_name = lambda name: (name + '.lib') if system == 'windows' else ('lib' + name + '.a')
    # Match the actual upstream combined archive: shaderc API/util, full glslang
    # (including HLSL/OGLCompiler/SPIRV) and both validation/optimization libraries.
    merged = [artifacts / library_name(name) for name in ('shaderc', 'glslang', 'SPIRV-Tools-opt', 'SPIRV-Tools')]
    if not all(path.is_file() for path in merged):
        raise RuntimeError('Premake shader archive output missing')
    destination = libdir / library_name('shaderc_combined')
    if system == 'windows':
        librarian = shutil.which('lib.exe')
        if not librarian:
            vswhere = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
            installation = Path(output(vswhere, '-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'))
            candidates = sorted(installation.glob('VC/Tools/MSVC/*/bin/Hostx64/x64/lib.exe'))
            if not candidates:
                raise RuntimeError('MSVC librarian unavailable for archive integration')
            librarian = candidates[-1]
        run(librarian, '/NOLOGO', f'/OUT:{destination}', *merged)
    else:
        # MRI filenames are simple; never interpolate checkout paths into shell
        # commands or depend on the MRI parser's path-with-spaces behavior.
        import tempfile
        with tempfile.TemporaryDirectory(prefix='hazel-shader-archive-') as directory:
            temporary = Path(directory)
            commands = ['CREATE shaderc_combined.a']
            for index, library in enumerate(merged):
                shutil.copyfile(library, temporary / f'input{index}.a')
                commands.append(f'ADDLIB input{index}.a')
            commands += ['SAVE', 'END']
            subprocess.run(['ar', '-M'], input='\n'.join(commands) + '\n', cwd=temporary, text=True, check=True)
            shutil.copyfile(temporary / 'shaderc_combined.a', destination)
    for name in ('spirv-cross-glsl', 'spirv-cross-core'):
        shutil.copyfile(artifacts / library_name(name), libdir / library_name(name))
    shutil.copytree(sources['shaderc'] / 'libshaderc/include/shaderc', include / 'shaderc', dirs_exist_ok=True)
    shutil.copytree(sources['spirv-tools'] / 'include/spirv-tools', include / 'spirv-tools', dirs_exist_ok=True)
    shutil.copytree(sources['spirv-headers'] / 'include/spirv', include / 'spirv', dirs_exist_ok=True)
    cross_headers = include / 'spirv_cross'
    cross_headers.mkdir(exist_ok=True)
    for header in (*sources['spirv-cross'].glob('*.hpp'), *sources['spirv-cross'].glob('*.h')):
        shutil.copyfile(header, cross_headers / header.name)
    for name, source in sources.items():
        if output('git', '-C', source, 'status', '--porcelain'):
            raise RuntimeError(f'Build modified dependency source: {name}')
        licenses = prefix / 'licenses' / name
        licenses.mkdir(parents=True, exist_ok=True)
        for license_file in source.glob('LICENSE*'):
            if license_file.is_file():
                shutil.copyfile(license_file, licenses / license_file.name)
    sys.path.insert(0, str(ROOT / 'scripts'))
    from hazel import dependency_digest
    manifest = dict(build_inputs_sha256=dependency_digest(), configuration=configuration, system=system, build_system='Premake',
                    pins_sha256=hashlib.sha256(PINS_FILE.read_bytes()).hexdigest(), pins=PINS,
                    premake=output(options.premake, f'--file={ROOT / "scripts/dependencies/shader-workspace.lua"}', '--version').splitlines()[0],
                    source_manifest_sha256=hashlib.sha256((ROOT / 'scripts/dependencies/shader-sources.json').read_bytes()).hexdigest(),
                    jobs=options.jobs, source_status='clean', windows_crt=('MDd' if configuration=='Debug' else 'MD') if system=='windows' else None)
    (prefix / 'shader-tools.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'Installed clean pinned Premake shader tools: {prefix}', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', choices=['Debug', 'Release', 'all'], default='all')
    parser.add_argument('--jobs', type=int, choices=[1, 2], default=2)
    parser.add_argument('--premake', default='premake5')
    parser.add_argument('--msbuild', default='msbuild')
    parser.add_argument('--prepare-only', action='store_true', help='Fetch/verify pins and generate inputs/projects without compilation')
    options = parser.parse_args()
    system = platform.system().lower()
    if system not in ('linux', 'windows') or platform.machine().lower() not in ('x86_64', 'amd64'):
        parser.error('Only the currently implemented Linux/Windows x86_64 builds are available')
    source_root = ROOT / 'build/dependencies/sources'
    sources = {'shaderc': source_root / 'shaderc', 'spirv-cross': source_root / 'spirv-cross'}
    for name in ('glslang', 'spirv-headers', 'spirv-tools'):
        sources[name] = sources['shaderc'] / 'third_party' / name
    for name, destination in sources.items():
        checkout(name, destination)
    run(sys.executable, ROOT / 'scripts/dependencies/generate-shader-inputs.py')
    run(options.premake, f'--file={ROOT / "scripts/dependencies/shader-workspace.lua"}', 'vs2022' if system == 'windows' else 'gmake', cwd=ROOT)
    if options.prepare_only:
        return
    configurations = ['Debug', 'Release'] if options.config == 'all' else [options.config]
    for configuration in configurations:
        build(configuration, options, sources, system)


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print(f'Shader tool setup failed: {error}', file=sys.stderr)
        sys.exit(1)
