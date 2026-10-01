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
    cmake_root = ROOT / 'build/dependencies/cmake' / f'{configuration}-{system}-x86_64'
    common = [f'-DCMAKE_BUILD_TYPE={configuration}',
              '-DCMAKE_POSITION_INDEPENDENT_CODE=ON',
              '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>DLL']
    if int(output(options.cmake, '--version').split()[2].split('.')[0]) >= 4:
        common.append('-DCMAKE_POLICY_VERSION_MINIMUM=3.5')
    # The upstream version generator falls back to a timestamp in shallow checkouts.
    build_env = dict(os.environ, SOURCE_DATE_EPOCH=output('git', '-C', sources['shaderc'], 'show', '-s', '--format=%ct', 'HEAD'))
    generator = ['-G', 'Visual Studio 17 2022', '-A', 'x64'] if system == 'windows' else ['-G', 'Unix Makefiles']
    for name in ('shaderc', 'spirv-cross'):
        directory = cmake_root / name
        if name == 'shaderc':
            settings = [f'-DCMAKE_PROJECT_shaderc_INCLUDE={ROOT / "scripts/dependencies/shaderc-compat.cmake"}',
                        '-DSHADERC_SKIP_TESTS=ON', '-DSHADERC_SKIP_EXAMPLES=ON',
                        '-DSHADERC_SKIP_COPYRIGHT_CHECK=ON', '-DSHADERC_SKIP_INSTALL=ON',
                        '-DSHADERC_ENABLE_WERROR_COMPILE=OFF', '-DSHADERC_ENABLE_SHARED_CRT=ON',
                        '-DSPIRV_SKIP_EXECUTABLES=ON', '-DENABLE_GLSLANG_BINARIES=OFF',
                        '-DENABLE_GLSLANG_JS=OFF', '-DBUILD_TESTING=OFF']
            targets = ['shaderc_combined_genfile']
        else:
            settings = ['-DSPIRV_CROSS_CLI=OFF', '-DSPIRV_CROSS_ENABLE_TESTS=OFF',
                        '-DSPIRV_CROSS_SKIP_INSTALL=ON']
            targets = ['spirv-cross-glsl']
        run(options.cmake, '-S', sources[name], '-B', directory, *generator, *common, *settings)
        run(options.cmake, '--build', directory, '--config', configuration,
            '--target', *targets, '--parallel', options.jobs, env=build_env)
        libraries = ['shaderc_combined'] if name == 'shaderc' else ['spirv-cross-glsl', 'spirv-cross-core']
        for library in libraries:
            filename = f'{library}.lib' if system == 'windows' else f'lib{library}.a'
            artifact = filename
            if system == 'windows' and configuration == 'Debug' and name == 'spirv-cross':
                artifact = f'{library}d.lib'  # Cross's native MSVC Debug postfix.
            matches = list(directory.rglob(artifact))
            if len(matches) != 1:
                raise RuntimeError(f'Expected one {filename}, found {matches}')
            shutil.copy2(matches[0], libdir / filename)
        if name == 'shaderc':
            shutil.copytree(sources[name] / 'libshaderc/include/shaderc', include / 'shaderc', dirs_exist_ok=True)
        else:
            destination = include / 'spirv_cross'
            destination.mkdir(exist_ok=True)
            for header in (*sources[name].glob('*.hpp'), *sources[name].glob('*.h')):
                shutil.copy2(header, destination / header.name)
        licenses = prefix / 'licenses' / name
        licenses.mkdir(parents=True, exist_ok=True)
        for license_file in sources[name].glob('LICENSE*'):
            if license_file.is_file():
                shutil.copy2(license_file, licenses / license_file.name)
    for name, source in sources.items():
        if output('git', '-C', source, 'status', '--porcelain'):
            raise RuntimeError(f'Build modified dependency source: {name}')
        # Keep attribution for every bundled library, not only the public wrappers.
        licenses = prefix / 'licenses' / name
        licenses.mkdir(parents=True, exist_ok=True)
        for license_file in source.glob('LICENSE*'):
            if license_file.is_file():
                shutil.copy2(license_file, licenses / license_file.name)
    manifest = dict(configuration=configuration, system=system,
                    pins_sha256=hashlib.sha256(PINS_FILE.read_bytes()).hexdigest(),
                    pins=PINS, cmake=output(options.cmake, '--version').splitlines()[0],
                    jobs=options.jobs, source_status='clean')
    (prefix / 'shader-tools.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'Installed clean pinned shader tools: {prefix}', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', choices=['Debug', 'Release', 'all'], default='all')
    parser.add_argument('--jobs', type=int, choices=[1, 2], default=2)
    parser.add_argument('--cmake', default='cmake')
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
    configurations = ['Debug', 'Release'] if options.config == 'all' else [options.config]
    for configuration in configurations:
        build(configuration, options, sources, system)


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print(f'Shader tool setup failed: {error}', file=sys.stderr)
        sys.exit(1)
