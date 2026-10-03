#!/usr/bin/env python3
"""Hazel setup, incremental builds, launch, managed scripts, tests and packaging."""
import argparse
import contextlib
import hashlib
import json
import os
from pathlib import Path
import platform
import shlex
import shutil
import subprocess
import sys
import urllib.request

ROOT = Path(__file__).resolve().parent.parent
SYSTEM = 'windows' if os.name == 'nt' else 'linux'
PINS = json.loads((ROOT / 'scripts/internal/toolchain.json').read_text())


def run(args, cwd=ROOT, env=None, **kwargs):
    print('+', shlex.join(map(str, args)), flush=True)
    return subprocess.run(list(map(str, args)), cwd=cwd, env=env, check=True, **kwargs)


def output(args, **kwargs):
    return subprocess.check_output(list(map(str, args)), text=True, **kwargs).strip()


def vs_toolchain():
    vswhere = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    if not vswhere.is_file():
        raise RuntimeError('Install Visual Studio 2022 Build Tools with C++ x64/x86 tools, Windows SDK and .NET 4.7.2 targeting pack. The IDE is optional.')
    location = output([vswhere, '-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'])
    if not location:
        raise RuntimeError('Visual Studio C++ Build Tools were not found by vswhere')
    install = Path(location)
    msbuild = install / 'MSBuild/Current/Bin/MSBuild.exe'
    if not msbuild.is_file():
        raise RuntimeError('Install the MSBuild component of Visual Studio Build Tools')
    return install, msbuild


def diagnose():
    if platform.machine().lower() not in ('x86_64', 'amd64'):
        raise RuntimeError('This milestone supports x86_64 only')
    required = ['git'] if SYSTEM == 'windows' else ['git', 'make', 'g++', 'pkg-config', 'ar', 'tar']
    missing = [name for name in required if not shutil.which(name)]
    if missing:
        raise RuntimeError('Missing prerequisites: ' + ', '.join(missing) + '. See README prerequisite packages; setup never installs system packages.')
    if SYSTEM == 'windows':
        vs_toolchain()
        framework = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Reference Assemblies/Microsoft/Framework/.NETFramework/v4.7.2/mscorlib.dll'
        if not framework.is_file():
            raise RuntimeError('Install .NET Framework 4.7.2 Developer/Targeting Pack')
    else:
        for name in ('gtk+-3.0', 'x11', 'xrandr', 'xinerama', 'xcursor', 'xi', 'gl', 'uuid'):
            if subprocess.run(['pkg-config', '--exists', name]).returncode:
                raise RuntimeError(f'Missing {name} development files. See README Ubuntu/Arch prerequisite commands.')


def premake():
    owned = ROOT / 'build/tools/premake-core'
    executable = owned / ('bin/release/premake5.exe' if SYSTEM == 'windows' else 'bin/release/premake5')
    if not owned.exists():
        run(['git', 'clone', '--filter=blob:none', 'https://github.com/premake/premake-core.git', owned])
    if output(['git', '-C', owned, 'status', '--porcelain']):
        raise RuntimeError('Project-owned Premake checkout has modifications; preserve them before setup')
    if output(['git', '-C', owned, 'rev-parse', 'HEAD']) != PINS['premake']:
        run(['git', '-C', owned, 'fetch', 'origin', PINS['premake']])
        run(['git', '-C', owned, 'checkout', '--detach', PINS['premake']])
    if not executable.is_file():
        if SYSTEM == 'windows':
            install, _ = vs_toolchain()
            bootstrap = ROOT / 'build/tools/bootstrap-windows.cmd'
            bootstrap.write_text('@echo off\ncall "' + str(install / 'VC/Auxiliary/Build/vcvars64.bat') + '"\nif errorlevel 1 exit /b 1\nnmake -f Bootstrap.mak windows-msbuild PLATFORM=x64 MSDEV=vs2022 PREMAKE_OPTS=--curl-src=none\nexit /b %errorlevel%\n', encoding='utf-8')
            run(['cmd', '/d', '/c', bootstrap], cwd=owned)
        else:
            bootstrap = ROOT / 'build/tools/PremakeBootstrap.mak'
            bootstrap.write_text((owned/'Bootstrap.mak').read_text().replace('-j`getconf _NPROCESSORS_ONLN`', '-j2'))
            run(['make', '-f', bootstrap, 'linux', '-j2', 'PREMAKE_OPTS=--curl-src=none'], cwd=owned)
    return executable


def mono_prefix():
    if SYSTEM == 'windows':
        return ROOT / 'Hazel/vendor/mono'
    explicit = os.environ.get('HAZEL_MONO_SDK')
    if explicit:
        prefix = Path(explicit).resolve()
    else:
        local = ROOT / 'build/dependencies/mono/linux/usr'
        prefix = local if local.is_dir() else Path('/usr')
    required = ['include/mono-2.0/mono/jit/jit.h', 'lib/libmonosgen-2.0.so',
                'lib/mono/4.5/mscorlib.dll', 'lib/mono/4.5/mcs.exe', 'lib/mono/4.7.2-api/mscorlib.dll']
    if all((prefix / name).is_file() for name in required):
        return prefix
    if prefix != Path('/usr') or explicit:
        raise RuntimeError('Incomplete Mono SDK: ' + str(prefix))
    # Arch/CachyOS may no longer offer Mono. Bootstrap the recorded, checksum
    # pinned SDK in this checkout only; never alter /usr or use administrator access.
    if not shutil.which('bsdtar'):
        raise RuntimeError('Mono SDK unavailable. Install mono-devel (Ubuntu), or libarchive bsdtar (Arch) for the isolated pinned SDK. HAZEL_MONO_SDK may select an existing SDK.')
    pin = json.loads((ROOT / 'scripts/dependencies/mono.json').read_text())['linux_local']
    archive = ROOT / 'build/downloads' / pin['package']
    archive.parent.mkdir(parents=True, exist_ok=True)
    if not archive.exists():
        urllib.request.urlretrieve(pin['url'], archive)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != pin['sha256']:
        raise RuntimeError('Pinned Mono SDK checksum mismatch')
    destination = ROOT / 'build/dependencies/mono/linux'
    destination.mkdir(parents=True, exist_ok=True)
    run(['bsdtar', '-xf', archive, '-C', destination])
    return mono_prefix()


def mono_command(prefix):
    if SYSTEM == 'windows':
        return []
    config = (prefix / '../etc/mono/config').resolve() if prefix != Path('/usr') else Path('/etc/mono/config')
    return [prefix / 'bin/mono', '--config', config, prefix / 'lib/mono/4.5/mcs.exe']


def dependency_digest():
    digest = hashlib.sha256()
    for name in ('build-shader-tools.py', 'shader-workspace.lua', 'generate-shader-inputs.py',
                 'shader-sources.json', 'shader-tools.json', 'shader-tools-compat.h'):
        digest.update(name.encode()); digest.update((ROOT / 'scripts/dependencies' / name).read_bytes())
    return digest.hexdigest()


def dependencies(configuration, generator):
    sdk = ROOT / 'build/dependencies/install' / f'{configuration}-{SYSTEM}-x86_64'
    try:
        manifest = json.loads((sdk / 'shader-tools.json').read_text())
        pins = json.loads((ROOT / 'scripts/dependencies/shader-tools.json').read_text())
        libraries = [('' if SYSTEM == 'windows' else 'lib') + name + ('.lib' if SYSTEM == 'windows' else '.a') for name in ('shaderc_combined', 'spirv-cross-core', 'spirv-cross-glsl')]
        ready = (manifest['pins'] == pins and manifest['configuration'] == configuration and
                 manifest['system'] == SYSTEM and manifest['source_status'] == 'clean' and
                 manifest['build_system'] == 'Premake' and manifest.get('build_inputs_sha256') == dependency_digest() and
                 all((sdk / 'lib' / name).is_file() for name in libraries))
    except (OSError, KeyError, ValueError):
        ready = False
    if not ready:
        command = [sys.executable, ROOT / 'scripts/dependencies/build-shader-tools.py', '--config', configuration, '--jobs', '2', '--premake', generator]
        if SYSTEM == 'windows': command += ['--msbuild', vs_toolchain()[1]]
        run(command)
    else:
        print(f'Using current {configuration} shader dependencies.', flush=True)


def build(configuration, tests=False, database=False):
    generator = premake()
    prefix = mono_prefix()
    dependencies(configuration, generator)
    args = [generator]
    if SYSTEM == 'linux': args += [f'--mono-root={prefix}']
    if tests: args += ['--migration-tests', '--shader-tools']
    args += ['vs2022' if SYSTEM == 'windows' else 'gmake']
    run(args)
    if SYSTEM == 'windows':
        command = [vs_toolchain()[1], ROOT / 'Hazel.sln', '/m:2', f'/p:Configuration={configuration}', '/p:Platform=x64', '/verbosity:minimal']
    else:
        command = ['make', f'config={configuration.lower()}', '-j2', 'CSC=' + shlex.join(map(str, mono_command(prefix)))]
        numbers = tuple(map(int, output(['make', '--version']).splitlines()[0].split()[-1].split('.')[:2]))
        if numbers >= (4, 4): command += ['--jobserver-style=pipe']
    if database:
        if SYSTEM == 'windows': raise RuntimeError('Bear refresh is supported on Linux')
        if not shutil.which('bear'): raise RuntimeError('Install Bear to refresh clangd compiler commands')
        command = ['bear', '--output', ROOT / 'compile_commands.json', '--', *command, '--always-make']
    run(command)
    if database:
        commands = json.loads((ROOT / 'compile_commands.json').read_text())
        recorded = {(Path(item['directory']) / item['file']).resolve() for item in commands}
        for source_root in ('Hazel/src', 'Hazelnut/src', 'Nutella/src'):
            if not any(path.is_relative_to(ROOT / source_root) for path in recorded):
                raise RuntimeError('Bear did not capture compiler commands for ' + source_root)
        print('Refreshed clangd database: ' + str(len(commands)) + ' compiler commands.', flush=True)
    stage(configuration, prefix)


def copy_changed(source, destination):
    destination.parent.mkdir(parents=True, exist_ok=True)
    if not destination.is_file() or source.read_bytes() != destination.read_bytes():
        shutil.copy2(source, destination)


def copy_tree(source, destination):
    for path in source.rglob('*'):
        if path.is_file(): copy_changed(path, destination / path.relative_to(source))


def binaries(configuration):
    return ROOT / 'bin' / f'{configuration}-{SYSTEM}-x86_64'


def stage(configuration, prefix=None):
    prefix = prefix or mono_prefix()
    base = binaries(configuration)
    for name in ('Hazelnut', 'Nutella'):
        app = base / name
        copy_tree(ROOT / 'Hazel/Resources', app / 'Resources')
        if name == 'Hazelnut':
            copy_tree(ROOT / 'Hazelnut/Resources/Icons', app / 'Resources/Icons')
            copy_changed(ROOT / 'Hazelnut/Resources/imgui.ini', app / 'Resources/imgui.ini')
        copy_changed(base / 'Hazel-ScriptCore/Hazel-ScriptCore.dll', app / 'Resources/Scripts/Hazel-ScriptCore.dll')
        # Development runtime is staged with the same roots as distribution.
        if SYSTEM == 'windows':
            copy_tree(ROOT / 'Hazelnut/mono/lib', app / 'mono/lib')
            copy_changed(ROOT / 'scripts/internal/mono-windows.config', app / 'mono/etc/mono/config')
        else:
            mono = app / 'mono'; mono.mkdir(exist_ok=True)
            for target, source in ((mono / 'lib', prefix / 'lib'), (mono / 'etc', (prefix / '../etc').resolve() if prefix != Path('/usr') else Path('/etc'))):
                if not target.exists(): target.symlink_to(source, target_is_directory=True)
            lib = app / 'lib'; lib.mkdir(exist_ok=True)
            for source in (prefix / 'lib').glob('libmono*.so*'):
                target = lib / source.name
                if not target.exists(): target.symlink_to(source.resolve())
    copy_changed(base / 'ExampleScripts/SceneTransitions.dll', ROOT / 'examples/SceneTransitions/Assets/Scripts/Binaries/SceneTransitions.dll')


def script_build(project, configuration):
    from internal.packaging import load_project
    descriptor, config, assets = load_project(project)
    scripts = assets / 'Scripts'
    if not (scripts / 'premake5.lua').is_file():
        raise RuntimeError('Project needs Assets/Scripts/premake5.lua; see the bundled example')
    generator = premake(); prefix = mono_prefix()
    env = os.environ.copy()
    env['HAZEL_SCRIPTCORE'] = str(binaries(configuration) / 'Hazel-ScriptCore/Hazel-ScriptCore.dll')
    if not Path(env['HAZEL_SCRIPTCORE']).is_file(): raise RuntimeError('Build Hazel first to supply Hazel-ScriptCore')
    run([generator, 'vs2022' if SYSTEM == 'windows' else 'gmake'], cwd=scripts, env=env)
    if SYSTEM == 'windows':
        run([vs_toolchain()[1], scripts / (config['Name'] + '.sln'), '/m:2', f'/p:Configuration={configuration}', '/p:Platform=x64'], env=env)
    else:
        run(['make', f'config={configuration.lower()}', '-j2', 'CSC=' + shlex.join(map(str, mono_command(prefix)))], cwd=scripts, env=env)
    module = assets / config['ScriptModulePath'].replace('\\', '/')
    if not module.is_file(): raise RuntimeError('Script build did not produce descriptor ScriptModulePath: ' + str(module))


@contextlib.contextmanager
def build_lock():
    path = ROOT / 'build/tooling.lock'; path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('a+b') as handle:
        if SYSTEM == 'windows':
            import msvcrt
            handle.seek(0); handle.write(b'0'); handle.flush(); handle.seek(0)
            msvcrt.locking(handle.fileno(), msvcrt.LK_LOCK, 1)
        else:
            import fcntl
            fcntl.flock(handle, fcntl.LOCK_EX)
        try: yield
        finally:
            if SYSTEM == 'windows': handle.seek(0); msvcrt.locking(handle.fileno(), msvcrt.LK_UNLCK, 1)
            else: fcntl.flock(handle, fcntl.LOCK_UN)


def yaml_tools():
    # Package tooling owns its Python dependency; no global pip installs.
    target = ROOT / 'build/tools/python'
    sys.path.insert(0, str(target))
    try:
        import yaml
        if yaml.__version__ == PINS['pyyaml']: return
    except ImportError: pass
    if subprocess.run([sys.executable, '-m', 'pip', '--version'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode:
        raise RuntimeError('Package tooling needs Python pip or PyYAML '+PINS['pyyaml']+'. Install python3-pip (Ubuntu) / python-pip (Arch) explicitly, then rerun setup.')
    run([sys.executable, '-m', 'pip', 'install', '--disable-pip-version-check', '--target', target, '--upgrade', 'PyYAML==' + PINS['pyyaml']])
    # Subsequent commands load the project-owned version in a fresh interpreter.
    for name in list(sys.modules):
        if name == 'yaml' or name.startswith('yaml.'): del sys.modules[name]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='action', required=True)
    for action in ('setup', 'build', 'database', 'test'):
        p = sub.add_parser(action)
        p.add_argument('--config', choices=('Debug', 'Release'), default='Debug')
        if action in ('setup', 'build'): p.add_argument('--tests', action='store_true')
        if action == 'test': p.add_argument('--profile', choices=('native','gl41','software'), default='software' if SYSTEM=='windows' else 'native')
    p = sub.add_parser('run'); p.add_argument('app', choices=('Hazelnut', 'Nutella')); p.add_argument('--config', choices=('Debug', 'Release'), default='Debug'); p.add_argument('--project', type=Path)
    p = sub.add_parser('script-build'); p.add_argument('project', type=Path); p.add_argument('--config', choices=('Debug', 'Release'), default='Debug')
    p = sub.add_parser('test-packages'); p.add_argument('--output', type=Path, default=ROOT/'dist'); p.add_argument('--profile', choices=('native','gl41','software'), default='software' if SYSTEM=='windows' else 'native')
    p = sub.add_parser('package'); p.add_argument('--app', choices=('Hazelnut', 'Nutella', 'all'), default='all'); p.add_argument('--project', type=Path, default=ROOT/'examples/SceneTransitions/SceneTransitions.hproj'); p.add_argument('--output', type=Path, default=ROOT/'dist'); p.add_argument('--external-assets', choices=('reject',), default='reject', help='External references must be moved into the project and saved before packaging')
    args = parser.parse_args()
    if args.action == 'run':
        with build_lock(): stage(args.config)
        exe = binaries(args.config) / args.app / (args.app + ('.exe' if SYSTEM == 'windows' else ''))
        command = [exe]
        if args.project: command += (['--project'] if args.app == 'Nutella' else []) + [args.project.resolve()]
        elif args.app == 'Nutella': command += ['--project', ROOT/'examples/SceneTransitions/SceneTransitions.hproj']
        run(command, cwd=Path.cwd())
        return
    with build_lock():
        if args.action in ('setup', 'build', 'database'):
            if args.action == 'setup':
                diagnose(); run(['git', 'submodule', 'update', '--init', '--recursive']); yaml_tools()
            build(args.config, getattr(args, 'tests', False), args.action == 'database')
            if args.action == 'setup': print('Ready. python scripts/hazel.py run Hazelnut --project examples/SceneTransitions/SceneTransitions.hproj\nF5: copy scripts/internal/vscode/' + SYSTEM + '/ templates into .vscode. Use hazel.py --help for all workflows.')
        elif args.action == 'script-build':
            yaml_tools(); script_build(args.project, args.config)
        elif args.action == 'package':
            yaml_tools()
            from internal.packaging import package
            package(args.app, args.project, args.output)
        elif args.action == 'test-packages':
            from internal.package_tests import package_tests
            package_tests(args.output.resolve(), args.profile)
        elif args.action == 'test':
            from internal.tests import test
            test(args.config, args.profile)


if __name__ == '__main__':
    try: main()
    except (RuntimeError, OSError, subprocess.CalledProcessError) as error:
        print('Hazel: ' + str(error), file=sys.stderr); sys.exit(1)
