"""Native content creation frontend; Python remains for compiler/package operations."""
from pathlib import Path
import os
import subprocess
import hazel as hz


def native_generator():
    # Only binaries in this explicitly selected SDK, in a fixed bounded order.
    suffix = '.exe' if hz.SYSTEM == 'windows' else ''
    for config in ('Debug', 'Release'):
        path = hz.binaries(config) / 'HazelProject' / ('HazelProject' + suffix)
        if path.is_file():
            return path
    raise RuntimeError('SDK native HazelProject generator is missing; run SDK setup/build. '
                       'No duplicate Python template fallback is provided.')


def native_contract(configuration='Debug'):
    env = os.environ.copy()
    if hz.SYSTEM == 'linux':
        env['LD_LIBRARY_PATH'] = str(hz.mono_prefix() / 'lib')
    probe = subprocess.run([str(native_generator()), '--contract'], cwd=hz.ROOT, env=env,
                           capture_output=True, text=True, encoding='utf-8', timeout=10)
    if probe.returncode or probe.stdout.strip() != 'HAZEL_AUTHORING_CONTRACT=1':
        raise RuntimeError('Incompatible native generator contract; rebuild matching SDK tools')
    audit = hz.binaries(configuration) / 'PackageAudit/PackageAudit.exe'
    core = hz.binaries(configuration) / 'Hazel-ScriptCore/Hazel-ScriptCore.dll'
    if not audit.is_file() or not core.is_file():
        raise RuntimeError('Missing ' + configuration + ' ScriptCore/contract validator; '
                           'build matching SDK tools')
    # mono_command ends in mcs.exe: this probe uses the runtime, not the compiler.
    runtime = hz.mono_command(hz.mono_prefix())[:-1]
    probe = subprocess.run(list(map(str, [*runtime, audit, '--core-contract', core])),
                           cwd=hz.ROOT, env=env, capture_output=True, text=True,
                           encoding='utf-8', timeout=15)
    if probe.returncode or 'HAZEL_SCRIPTCORE_CONTRACT=1' not in probe.stdout:
        raise RuntimeError(probe.stderr.strip() or
                           'Incompatible ScriptCore API contract; rebuild matching SDK tools')


def create_project(name, technical, destination, build=False):
    destination = destination.resolve()
    env = os.environ.copy()
    if hz.SYSTEM == 'linux':
        env['LD_LIBRARY_PATH'] = str(hz.mono_prefix() / 'lib')
    command = [native_generator(), 'create', '--name', name, '--identifier', technical,
               '--destination', destination, '--templates', hz.ROOT / 'Hazel/Resources/Templates']
    result = subprocess.run(list(map(str, command)), cwd=hz.ROOT, env=env,
                            capture_output=True, text=True, encoding='utf-8', timeout=30)
    if result.returncode:
        raise RuntimeError(result.stderr.strip() or 'Native project creation failed')
    print(result.stdout, end='', flush=True)
    descriptor = destination / (technical + '.hproj')
    if build:
        # Creation remains published/editable if compilation fails.
        hz.script_build(descriptor, 'Debug')
    return descriptor


def preflight(operation):
    native_contract('Debug')
    if hz.SYSTEM=='windows':
        hz.vs_toolchain(operation=='export')
        targeting=Path(os.environ.get('ProgramFiles(x86)','C:/Program Files (x86)'))/'Reference Assemblies/Microsoft/Framework/.NETFramework/v4.7.2/mscorlib.dll'
        if not targeting.is_file(): raise RuntimeError('Install .NET Framework 4.7.2 targeting pack for script builds')
    else:
        prefix=hz.ROOT/'build/dependencies/mono/linux/usr'
        if not (prefix/'lib/mono/4.5/mcs.exe').is_file() and not Path('/usr/lib/mono/4.5/mcs.exe').is_file():
            raise RuntimeError('Mono compiler SDK unavailable. Run SDK setup once or install mono-devel; precompiled Play still works.')
    if not (hz.binaries('Debug')/'Hazel-ScriptCore/Hazel-ScriptCore.dll').is_file():
        raise RuntimeError('SDK Debug ScriptCore is missing. Build/setup this SDK first; script builds need matching ScriptCore.')
    premake=hz.ROOT/'build/tools/premake-core/bin/release'/('premake5.exe' if hz.SYSTEM=='windows' else 'premake5')
    if not premake.is_file(): raise RuntimeError('SDK Premake is missing. Run SDK setup before editor builds.')
    if operation=='export': hz.diagnose()
    print('READY: Python, configured SDK, Premake and script compiler'+(' and host native compiler' if operation=='export' else '')+' available.',flush=True)
