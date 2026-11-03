"""Native arm64 application bundles; source SDKs remain separate authoring inputs."""
import hashlib
import json
import os
from pathlib import Path
import plistlib
import shutil
import subprocess
import tempfile

import hazel as hz


def output(*args):
    return subprocess.check_output(list(map(str,args)),text=True).strip()


def dependencies(binary):
    return [line.strip().split(' (',1)[0] for line in output('otool','-L',binary).splitlines()[1:]]


def audit_binary(binary, native):
    if output('lipo','-archs',binary).split()!=['arm64']:
        raise RuntimeError('Package binary is not native arm64: '+str(binary))
    lines=output('otool','-l',binary).splitlines()
    minima=[line.split()[1] for line in lines if line.strip().startswith('minos ')]
    if not minima or any(tuple(map(int,version.split('.')))>(27,0) for version in minima):
        raise RuntimeError('Package binary exceeds macOS 27.0 floor: '+str(binary))
    for dependency in dependencies(binary):
        if dependency.startswith(('/System/Library/','/usr/lib/')):continue
        if dependency.startswith('@rpath/') and (native/Path(dependency).name).is_file():continue
        raise RuntimeError('Unresolved or producer-dependent Mach-O load: '+dependency+' in '+str(binary))
    # Rpaths are a second independent source of accidental producer dependencies.
    for index,line in enumerate(lines):
        if line.strip()=='cmd LC_RPATH':
            path=next(v.strip().split(' ',1)[1].split(' (offset',1)[0] for v in lines[index:index+5] if v.strip().startswith('path '))
            if not path.startswith(('@executable_path/','@loader_path/')):
                raise RuntimeError('Absolute/unowned package rpath: '+path)
    return {'architecture':'arm64','minimum_macos':minima,'dependencies':dependencies(binary)}


def sign(path):
    hz.run(['codesign','--force','--sign','-','--timestamp=none',path])


def relocate_executable(executable):
    # Build products use the loose development layout. Only the staged copy is
    # rewritten; generated Xcode settings remain useful for ordinary IDE runs.
    hz.run(['install_name_tool','-rpath','@executable_path/mono/lib',
            '@executable_path/../Frameworks',executable])


def package(app, project, destination, archive_name=None):
    from internal.packaging import load_project,validate_project,validate_engine_fonts,copy_project,licenses
    descriptor,config,assets=load_project(project)
    validate_project(descriptor,config,assets);validate_engine_fonts(hz.ROOT/'Hazel/Resources')
    destination=destination.resolve();destination.mkdir(parents=True,exist_ok=True)
    sdk=hz.mono_prefix();commit=hz.output(['git','-C',hz.ROOT,'rev-parse','HEAD'])
    provenance=json.loads((sdk.parent/'candidate-manifest.json').read_text())
    for name in (('Hazelnut','Nutella') if app=='all' else (app,)):
        tag=f'{archive_name or name}-macos-arm64-Release'
        with tempfile.TemporaryDirectory(prefix='.package-',dir=destination) as temporary:
            root=Path(temporary)/tag;root.mkdir()
            bundle=root/(name+'.app');contents=bundle/'Contents'
            binary=contents/'MacOS'/name;binary.parent.mkdir(parents=True)
            hz.copy_changed(hz.binaries('Release')/name/name,binary)
            resources=contents/'Resources';native=contents/'Frameworks';mono=resources/'Mono'
            hz.copy_tree(hz.ROOT/'Hazel/Resources',resources)
            if name=='Hazelnut':
                hz.copy_tree(hz.ROOT/'Hazelnut/Resources/Icons',resources/'Icons')
                hz.copy_changed(hz.ROOT/'Hazelnut/Resources/imgui.ini',resources/'imgui.ini')
            hz.copy_changed(hz.binaries('Release')/'Hazel-ScriptCore/Hazel-ScriptCore.dll',resources/'Scripts/Hazel-ScriptCore.dll')
            # Include only the runtime profile/GAC, not mcs, reference packs,
            # embedding headers or native static archives from the source SDK.
            for source in (sdk/'lib/mono').rglob('*'):
                if source.is_file() and source.suffix.lower() in ('.dll','.config') and not any(p.endswith('-api') for p in source.parts):
                    hz.copy_changed(source,mono/'lib/mono'/source.relative_to(sdk/'lib/mono'))
            hz.copy_tree(sdk/'etc/mono',mono/'etc/mono')
            # Profiles/config are resources; Frameworks contains native code
            # only. Mono's dynamic helper mappings use the same audited rpath
            # as the embedding executable rather than a producer lib directory.
            config_file=mono/'etc/mono/config'
            config_file.write_text(config_file.read_text().replace('$mono_libdir/','@rpath/'))
            for source in (sdk/'lib').glob('*.dylib'):
                hz.copy_changed(source.resolve(),native/source.name)
            relocate_executable(binary)
            copy_project(descriptor,assets,resources/'Project')
            licenses(resources)
            for notice in ('LICENSE','COPYING.LIB'):
                hz.copy_changed(sdk/notice,resources/'licenses/Mono-candidate'/notice)
            (resources/'licenses/Mono-candidate/provenance.json').write_text(json.dumps(provenance,indent=2)+'\n')
            with (contents/'Info.plist').open('wb') as stream:
                plistlib.dump({'CFBundleIdentifier':'com.kadencringle.hazel.'+name.lower(),
                    'CFBundleName':name,'CFBundleDisplayName':name,'CFBundleExecutable':name,
                    'CFBundlePackageType':'APPL','CFBundleShortVersionString':'0.1',
                    'CFBundleVersion':str(int(commit[:8],16)), 'LSMinimumSystemVersion':'27.0',
                    'LSArchitecturePriority':['arm64'],'NSHighResolutionCapable':True,
                    'NSPrincipalClass':'NSApplication'},stream)
            closure={str(p.relative_to(contents)):audit_binary(p,native) for p in [binary,*sorted(native.glob('*.dylib'))]}
            metadata={'commit':commit,'configuration':'Release','platform':'macos','architecture':'arm64',
                      'minimum_macos':'27.0','project':descriptor.name,'signing':'local ad-hoc',
                      'native_dependencies':closure,'mono_source':provenance['source'],
                      'mono_producer':provenance['producer'],
                      'source_dirty':bool(hz.output(['git','-C',hz.ROOT,'status','--porcelain'])),
                      'shader_tools':json.loads((hz.ROOT/'build/dependencies/install/Release-macos-arm64/shader-tools.json').read_text())}
            (resources/'build.json').write_text(json.dumps(metadata,indent=2)+'\n')
            (root/'LAUNCH.txt').write_text(f'{name}: macOS 27.0+, Apple Silicon, real OpenGL 4.1.\n'
                f'Open {name}.app in Finder, or run its Contents/MacOS/{name} executable from any working directory.\n'
                'Prepared project and Mono runtime are included. Script authoring/export requires a separately prepared source SDK.\n'
                'Settings/cache/save data use writable platform user locations outside the bundle.\n'
                'Locally ad-hoc signed; no Developer ID, notarization or fresh-download Gatekeeper acceptance is claimed.\n',encoding='utf-8')
            # Every binary mutation and resource write precedes inside-out signing.
            for library in sorted(native.glob('*.dylib')):sign(library)
            sign(binary);sign(bundle)
            hz.run(['codesign','--verify','--deep','--strict',bundle])
            (root/'SHA256SUMS').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.relative_to(root).as_posix()+'\n' for p in sorted(root.rglob('*')) if p.is_file()))
            archive=Path(shutil.make_archive(str(Path(temporary)/tag),'zip',temporary,tag))
            target=destination/archive.name;os.replace(archive,target)
            target.with_name(target.name+'.sha256').write_text(hashlib.sha256(target.read_bytes()).hexdigest()+'  '+target.name+'\n')
            print('Packaged',target,flush=True)
