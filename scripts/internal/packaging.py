"""Relocatable Release distributions and descriptor-based project validation."""
import hashlib
import json
import os
import platform
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

import hazel as hz


def load_project(path):
    import yaml
    descriptor = path.resolve(strict=True)
    if descriptor.suffix != '.hproj': raise RuntimeError('Select a .hproj descriptor')
    try:
        data = yaml.safe_load(descriptor.read_text(encoding='utf-8'))
    except yaml.YAMLError as error: raise RuntimeError('Cannot parse project ' + str(descriptor) + ': ' + str(error)) from error
    config = data.get('Project') if isinstance(data, dict) else None
    if not isinstance(config, dict) or any(not isinstance(config.get(k), str) or not config[k] for k in ('Name', 'StartScene', 'AssetDirectory', 'ScriptModulePath')):
        raise RuntimeError('Project requires Name, StartScene, AssetDirectory and ScriptModulePath')
    assets = resolve_owned(descriptor.parent, config['AssetDirectory'])
    if not assets.is_dir(): raise RuntimeError('Missing asset root: ' + str(assets))
    for key in ('StartScene', 'ScriptModulePath'):
        resolve_owned(assets, config[key], exists=key!='ScriptModulePath')
    if Path(config['StartScene']).suffix != '.hazel': raise RuntimeError('StartScene must be a .hazel scene')
    if Path(config['ScriptModulePath']).suffix != '.dll': raise RuntimeError('ScriptModulePath must select a compiled managed DLL')
    return descriptor, config, assets


def resolve_owned(root, reference, exists=True):
    reference = reference.replace('\\', '/')
    path = Path(reference)
    if path.is_absolute() or re.match(r'^[A-Za-z]:/', reference):
        raise RuntimeError('External asset reference is not portable: ' + reference + '. Move it inside the project and save its relative reference.')
    resolved = (root / path).resolve(strict=exists)
    if not resolved.is_relative_to(root.resolve()):
        raise RuntimeError('Asset reference/symlink escapes its project root: ' + reference)
    return resolved


def validate_project(descriptor, config, assets):
    import yaml
    scenes = list(assets.rglob('*.hazel'))
    if not scenes: raise RuntimeError('No .hazel scenes in selected asset root')
    for file in assets.rglob('*'):
        if file.is_symlink() and not file.resolve().is_relative_to(assets): raise RuntimeError('External asset symlink: ' + str(file))
    for scene in scenes:
        content = yaml.safe_load(scene.read_text(encoding='utf-8'))
        if not isinstance(content, dict) or 'Scene' not in content: raise RuntimeError('Invalid scene: ' + str(scene))
        ids = set()
        for entity in content.get('Entities', []) or []:
            ident = entity['Entity']
            if ident in ids: raise RuntimeError('Duplicate entity UUID in ' + str(scene))
            ids.add(ident)
            texture = entity.get('SpriteRendererComponent', {}).get('TexturePath')
            if texture: resolve_owned(assets, texture)
    core = hz.binaries('Release') / 'Hazel-ScriptCore/Hazel-ScriptCore.dll'
    module = resolve_owned(assets, config['ScriptModulePath'])
    auditor = hz.binaries('Release') / 'PackageAudit/PackageAudit.exe'
    command = [auditor, core, module, assets]
    if hz.SYSTEM == 'linux': command = [hz.mono_prefix()/'bin/mono', *command]
    result = subprocess.run(list(map(str,command)), stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if result.returncode: raise RuntimeError(result.stderr.strip() or 'Managed dependency audit failed')
    known = {line.removeprefix('SCRIPT ') for line in result.stdout.splitlines() if line.startswith('SCRIPT ')}
    for scene in scenes:
        content = yaml.safe_load(scene.read_text(encoding='utf-8'))
        for entity in content.get('Entities', []) or []:
            name = entity.get('ScriptComponent', {}).get('ClassName')
            if name and name not in known: raise RuntimeError('Unavailable compiled script class ' + name + ' in ' + str(scene))
    print(result.stdout, end='')


# The dynamic loader, libc and the graphics-driver dispatch remain OS supplied.
# Everything else in the linked dependency closure is copied and accounted for.
LINUX_SYSTEM = {'libc.so.6', 'libm.so.6', 'libdl.so.2', 'libpthread.so.0', 'librt.so.1',
                'ld-linux-x86-64.so.2', 'libGL.so.1', 'libGLX.so.0', 'libGLdispatch.so.0'}
WINDOWS_SYSTEM = {name.lower() + '.dll' for name in ('KERNEL32', 'USER32', 'GDI32', 'ADVAPI32', 'WINMM', 'WS2_32', 'BCRYPT', 'CRYPT32', 'OLE32', 'OLEAUT32', 'SHELL32', 'PSAPI', 'VERSION', 'COMBASE', 'SHLWAPI', 'COMDLG32', 'RPCRT4', 'UCRTBASE', 'NTDLL', 'SECUR32', 'IMM32', 'SETUPAPI', 'MSWSOCK', 'IPHLPAPI', 'WINHTTP', 'NORMALIZ', 'OPENGL32', 'DWMAPI', 'USERENV')}


def linux_dependencies(binary):
    env = os.environ.copy()
    env['LD_LIBRARY_PATH'] = str(hz.mono_prefix() / 'lib')
    text = subprocess.check_output(['ldd', str(binary)], text=True, env=env)
    if 'not found' in text: raise RuntimeError('Unresolved native dependencies:\n' + text)
    result = {}
    for line in text.splitlines():
        match = re.match(r'\s*(\S+) => (/.+?) \(', line)
        if match: result[match[1]] = Path(match[2])
    return result


def linux_license(source, destination):
    # Package-manager metadata provides attribution for the actual copied libraries,
    # rather than a guessed list of dependencies.
    if shutil.which('dpkg-query'):
        candidates = [str(source), str(source.resolve())]
        package = None
        for path in candidates:
            result = subprocess.run(['dpkg-query', '-S', path], text=True, capture_output=True)
            if result.returncode == 0:
                package = result.stdout.split(': ')[0].split(':')[0]; break
        if not package: raise RuntimeError('Cannot identify redistribution license for ' + str(source))
        notice = Path('/usr/share/doc') / package / 'copyright'
        if not notice.is_file(): raise RuntimeError('Missing copyright notice for ' + package)
        hz.copy_changed(notice, destination / (package + '.copyright'))
    elif shutil.which('pacman'):
        package = hz.output(['pacman', '-Qqo', source.resolve()])
        notices = Path('/usr/share/licenses') / package
        if notices.is_dir(): hz.copy_tree(notices, destination / package)
        else:
            # Arch may use a common SPDX license instead of package-owned text.
            info = hz.output(['pacman', '-Qi', package])
            license_line = next((line for line in info.splitlines() if line.startswith('Licenses')), '')
            names = license_line.split(':', 1)[-1].split()
            common = Path('/usr/share/licenses/common')
            copied = False
            for name in names:
                if (common / name).is_dir(): hz.copy_tree(common/name, destination/package/name); copied = True
            if not copied: raise RuntimeError('Cannot identify license text for bundled ' + package + ' (' + license_line + ')')
    else: raise RuntimeError('Native redistribution license audit requires dpkg-query or pacman')


def native_closure(executable, package):
    records = {}
    if hz.SYSTEM == 'linux':
        prefix = hz.mono_prefix()
        private = package / 'mono/lib'; private.mkdir(parents=True, exist_ok=True)
        roots = [executable]
        for source in [*(prefix / 'lib').glob('libmono*.so*'), *(prefix / 'lib').glob('libMonoPosixHelper.so*')]:
            if source.is_file():
                hz.copy_changed(source.resolve(), private/source.name); roots.append(source.resolve())
        for root in roots:
            for soname, source in linux_dependencies(root).items():
                if soname in LINUX_SYSTEM:
                    records[soname] = {'provided_by': 'operating system / graphics driver'}; continue
                destination = package / 'lib' / soname
                hz.copy_changed(source.resolve(), destination)
                records[soname] = {'provided_by': 'package', 'sha256': hashlib.sha256(destination.read_bytes()).hexdigest()}
                if not soname.startswith('libmono'): linux_license(source, package/'licenses/native')
    else:
        install, _ = hz.vs_toolchain()
        redists = sorted(install.glob('VC/Redist/MSVC/*/x64/Microsoft.VC143.CRT'))
        if not redists: raise RuntimeError('Install the Visual C++ 2022 x64 redistributable files component')
        for source in redists[-1].glob('*.dll'): hz.copy_changed(source, package/source.name)
        inspectors = sorted(install.glob('VC/Tools/MSVC/*/bin/Hostx64/x64/dumpbin.exe'))
        if not inspectors: raise RuntimeError('dumpbin.exe is required for native dependency audit')
        roots = [executable, *package.glob('*.dll')]
        for root in roots:
            text = hz.output([inspectors[-1], '/DEPENDENTS', root])
            for name in re.findall(r'^\s+(\S+\.dll)\s*$', text, re.M | re.I):
                low = name.lower()
                if low.endswith('d.dll') and (low.startswith('msvcp') or low.startswith('vcruntime') or low.startswith('ucrt')):
                    raise RuntimeError('Debug CRT cannot be distributed: ' + name)
                if low in WINDOWS_SYSTEM or low.startswith(('api-ms-win-', 'ext-ms-win-')):
                    records[name] = {'provided_by': 'Windows 10 or newer'}
                elif (package/name).is_file(): records[name] = {'provided_by': 'package'}
                else: raise RuntimeError('Unresolved/non-OS native DLL: ' + name)
        (package/'licenses/VC-runtime.txt').write_text('Application-local Release Microsoft.VC143.CRT from the installed Visual Studio redistribution directory.\nhttps://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files\n', encoding='utf-8')
    return records


def licenses(package):
    hz.copy_changed(hz.ROOT/'LICENSE', package/'licenses/Hazel/LICENSE')
    hz.copy_changed(hz.ROOT/'Hazel/vendor/mono/LICENSE', package/'licenses/Mono/LICENSE')
    for source in (hz.ROOT/'Hazel/vendor').iterdir():
        if source.is_dir():
            for notice in source.rglob('*'):
                if notice.is_file() and notice.name.lower().startswith(('license', 'copying', 'copyright')):
                    hz.copy_changed(notice, package/'licenses/vendor'/source.name/notice.relative_to(source))
    for config in ('Release',):
        path = hz.ROOT/'build/dependencies/install'/f'{config}-{hz.SYSTEM}-x86_64/licenses'
        hz.copy_tree(path, package/'licenses/shader-tools')
    hz.copy_changed(hz.ROOT/'Hazel/Resources/fonts/opensans/LICENSE.txt', package/'licenses/OpenSans/LICENSE.txt')


def copy_project(descriptor, assets, destination):
    hz.copy_changed(descriptor, destination/descriptor.name)
    for notice in descriptor.parent.iterdir():
        if notice.is_file() and notice.name.lower().startswith(('license', 'copying', 'copyright', 'notice')):
            hz.copy_changed(notice, destination/notice.name)
    relative_root = assets.relative_to(descriptor.parent)
    for source in assets.rglob('*'):
        if not source.is_file(): continue
        relative = source.relative_to(assets)
        if any(part in ('Intermediates', 'bin', 'bin-int') for part in relative.parts): continue
        if source.suffix in ('.pdb', '.mdb', '.sln', '.csproj', '.make', '.user') or source.name == 'Makefile': continue
        hz.copy_changed(source, destination/relative_root/relative)


def package(app, project, output):
    descriptor, config, assets = load_project(project)
    validate_project(descriptor, config, assets)
    output = output.resolve(); output.mkdir(parents=True, exist_ok=True)
    prefix = hz.mono_prefix()
    commit = hz.output(['git', '-C', hz.ROOT, 'rev-parse', 'HEAD'])
    names = ('Hazelnut', 'Nutella') if app == 'all' else (app,)
    for name in names:
        tag = f'{name}-{hz.SYSTEM}-x86_64-Release'
        with tempfile.TemporaryDirectory(prefix='.package-', dir=output) as temporary:
            path = Path(temporary)/tag; path.mkdir()
            suffix = '.exe' if hz.SYSTEM == 'windows' else ''
            executable = hz.binaries('Release')/name/(name+suffix)
            if not executable.is_file(): raise RuntimeError('Build Release before packaging ' + name)
            hz.copy_changed(executable, path/executable.name)
            hz.copy_tree(hz.ROOT/'Hazel/Resources', path/'Resources')
            if name == 'Hazelnut':
                hz.copy_tree(hz.ROOT/'Hazelnut/Resources/Icons', path/'Resources/Icons')
                hz.copy_changed(hz.ROOT/'Hazelnut/Resources/imgui.ini', path/'Resources/imgui.ini')
            hz.copy_changed(hz.binaries('Release')/'Hazel-ScriptCore/Hazel-ScriptCore.dll', path/'Resources/Scripts/Hazel-ScriptCore.dll')
            assemblies = hz.ROOT/'Hazelnut/mono/lib/mono' if hz.SYSTEM == 'windows' else prefix/'lib/mono'
            # Runtime assemblies only; reference packs, mcs and build executables
            # belong to SDK setup, not application distribution.
            for source in assemblies.rglob('*'):
                if source.is_file() and source.suffix.lower() in ('.dll', '.config') and '-api' not in source.parts and not any(part.endswith('-api') for part in source.parts):
                    hz.copy_changed(source, path/'mono/lib/mono'/source.relative_to(assemblies))
            mono_config = hz.ROOT/'scripts/internal/mono-windows.config' if hz.SYSTEM == 'windows' else (prefix/'../etc/mono/config').resolve() if prefix != Path('/usr') else Path('/etc/mono/config')
            hz.copy_changed(mono_config, path/'mono/etc/mono/config')
            copy_project(descriptor, assets, path if name == 'Nutella' else path/'Example')
            licenses(path)
            closure = native_closure(executable, path)
            metadata = {'commit': commit, 'configuration': 'Release', 'platform': hz.SYSTEM, 'architecture': 'x86_64', 'project': descriptor.name, 'native_dependencies': closure, 'source_dirty': bool(hz.output(['git', '-C', hz.ROOT, 'status', '--porcelain'])), 'libc': list(platform.libc_ver()) if hz.SYSTEM=='linux' else None, 'shader_tools': json.loads((hz.ROOT/'build/dependencies/install'/f'Release-{hz.SYSTEM}-x86_64/shader-tools.json').read_text())}
            (path/'build.json').write_text(json.dumps(metadata, indent=2)+'\n')
            (path/'LAUNCH.txt').write_text(f'{name} Release x86_64\n'+
                ('Run Nutella (Nutella.exe on Windows) from any working directory. It discovers the root .hproj. --help and --list-projects need no graphics/Mono initialization.\n' if name=='Nutella' else 'Run Hazelnut (Hazelnut.exe on Windows). The bundled Example opens automatically; File > Open Project selects other projects.\n')+
                'OpenGL 4.1 core or newer hardware/driver is required. No bundled software driver.\nWindows: Windows 10 x64 or newer; Release CRT is app-local.\nLinux: compatibility is determined by the build host; official CI archives target Ubuntu 24.04 (glibc 2.39) and newer, X11/GLX display and system OpenGL driver. Local Arch builds require the local glibc baseline.\nPrecompiled scripts need no compiler. To author/compile scripts, install the Hazel source SDK prerequisites and use scripts/hazel.py script-build.\nWritable cache/settings: platform user-data/Hazel/'+name+'. HAZEL_DATA can select a writable directory.\n', encoding='utf-8')
            files = sorted(p for p in path.rglob('*') if p.is_file())
            (path/'SHA256SUMS').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.relative_to(path).as_posix()+'\n' for p in files))
            extension = 'zip' if hz.SYSTEM == 'windows' else 'gztar'
            archive = Path(shutil.make_archive(str(Path(temporary)/tag), extension, temporary, tag))
            target = output/archive.name
            os.replace(archive, target)
            checksum = hashlib.sha256(target.read_bytes()).hexdigest()
            target.with_name(target.name+'.sha256').write_text(checksum+'  '+target.name+'\n')
            print('Packaged', target, flush=True)
