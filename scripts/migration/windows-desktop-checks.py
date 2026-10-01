#!/usr/bin/env python3
"""Isolated Windows WGL/software regression checks; no system driver changes."""
import argparse
import ctypes as C
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
import urllib.request


def close_window(process, title):
    user = C.WinDLL('user32', use_last_error=True)
    callback_type = C.WINFUNCTYPE(C.c_bool, C.c_void_p, C.c_ssize_t)
    user.EnumWindows.argtypes = [callback_type, C.c_ssize_t]
    user.GetWindowThreadProcessId.argtypes = [C.c_void_p, C.POINTER(C.c_ulong)]
    user.GetWindowTextW.argtypes = [C.c_void_p, C.c_wchar_p, C.c_int]
    user.PostMessageW.argtypes = [C.c_void_p, C.c_uint, C.c_size_t, C.c_ssize_t]
    found = []
    @callback_type
    def inspect(window, unused):
        owner = C.c_ulong()
        user.GetWindowThreadProcessId(window, C.byref(owner))
        name = C.create_unicode_buffer(256)
        user.GetWindowTextW(window, name, len(name))
        if owner.value == process.pid and name.value == title:
            found.append(window)
        return True
    deadline = time.monotonic() + 90
    while time.monotonic() < deadline and process.poll() is None:
        user.EnumWindows(inspect, 0)
        if found:
            time.sleep(5)
            if not user.PostMessageW(found[0], 0x0010, 0, 0):
                raise RuntimeError('Cannot request graceful close of owned window')
            return process.wait(timeout=45)
        time.sleep(0.2)
    raise RuntimeError('Application window did not appear')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', required=True, choices=('Debug', 'Release'))
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This runner requires actual Windows; Linux execution is not Windows verification')
    root = Path(__file__).resolve().parents[2]
    pin = json.loads(Path(__file__).with_name('windows-mesa.json').read_text())
    evidence = root / 'build/migration/evidence'
    evidence.mkdir(parents=True, exist_ok=True)
    archive = root / 'build/migration/mesa3d-26.2.3-release-msvc.7z'
    if not archive.exists():
        urllib.request.urlretrieve(pin['url'], archive)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != pin['sha256']:
        raise RuntimeError('Windows Mesa archive checksum mismatch')
    extract = root / 'build/migration/windows-mesa'
    seven_zip = shutil.which('7z') or str(Path(os.environ['ProgramFiles']) / '7-Zip/7z.exe')
    subprocess.run([seven_zip, 'x', '-y', str(archive), '-o' + str(extract)], check=True)
    if not (extract / 'x64/opengl32.dll').exists():
        raise RuntimeError('Pinned Mesa package lacks x64 WGL driver')
    binaries = root / 'bin' / f'{args.config}-windows-x86_64'
    with tempfile.TemporaryDirectory(prefix='hazel-windows-render-') as directory:
        working = Path(directory)
        executable_dir = working / 'executables'
        executable_dir.mkdir()
        for dll in (extract / 'x64').glob('*.dll'):
            shutil.copyfile(dll, executable_dir / dll.name)
        for source in ('Sandbox', 'Hazelnut'):
            target = working / source
            target.mkdir()
            shutil.copytree(root / source / 'assets', target / 'assets')
            shutil.copyfile(root / source / 'imgui.ini', target / 'imgui.ini')
        editor = working / 'Hazelnut'
        shutil.copytree(root / 'Hazelnut/Resources', editor / 'Resources')
        shutil.copytree(root / 'Hazelnut/SandboxProject', editor / 'SandboxProject')
        project = editor / 'SandboxProject/project-é-🚀.hproj'
        shutil.copyfile(editor / 'SandboxProject/Sandbox.hproj', project)
        for profile in ('software46', 'software41'):
            checks = ['CoreSmoke', 'RendererFeaturesSmoke', 'Renderer2DSmoke', 'FontSmoke', 'SceneGPUSmoke', 'EditorSmoke', 'Sandbox', 'Hazelnut']
            if profile == 'software46':
                checks.insert(0, 'RendererSmoke')
            for name in checks:
                target = name if name in ('Sandbox', 'Hazelnut') else 'Migration' + name
                executable = executable_dir / (target + '.exe')
                shutil.copyfile(binaries / target / (target + '.exe'), executable)
                command = [str(executable)]
                if name == 'SceneGPUSmoke':
                    command += [str(binaries / 'Hazel-ScriptCore/Hazel-ScriptCore.dll'), str(binaries / 'MigrationManagedFixture/MigrationManagedFixture.dll')]
                elif name == 'EditorSmoke':
                    command += [str(binaries / 'Hazel-ScriptCore/Hazel-ScriptCore.dll'), str(binaries / 'SandboxScripts/Sandbox.dll'), str(root / 'Hazelnut')]
                elif name == 'Hazelnut':
                    command += [str(project)]
                environment = os.environ.copy()
                for variable in ('MESA_GL_VERSION_OVERRIDE', 'MESA_GLSL_VERSION_OVERRIDE', 'LIBGL_ALWAYS_SOFTWARE'):
                    environment.pop(variable, None)
                environment.update(GALLIUM_DRIVER='llvmpipe', LP_NUM_THREADS='2', MESA_SHADER_CACHE_DIR=str(working / 'mesa-cache'))
                if profile == 'software41':
                    environment.update(MESA_GL_VERSION_OVERRIDE='4.1', MESA_GLSL_VERSION_OVERRIDE='410')
                log = evidence / f'windows-{args.config.lower()}-{profile}-{name}.log'
                print(f'RUN {args.config} {profile} {name}', flush=True)
                with log.open('w') as output:
                    process = subprocess.Popen(command, cwd=editor if name == 'Hazelnut' else working / 'Sandbox', env=environment, stdout=output, stderr=subprocess.STDOUT)
                    try:
                        code = close_window(process, 'Hazelnut' if name == 'Hazelnut' else 'Hazel Engine') if name in ('Sandbox', 'Hazelnut') else process.wait(timeout=180)
                    finally:
                        if process.poll() is None:
                            process.kill(); process.wait()
                print(f'EXIT {code}', flush=True)
                if code:
                    print(log.read_text(errors='replace'), flush=True)
                    return code
                if name in ('CoreSmoke', 'RendererFeaturesSmoke', 'Renderer2DSmoke', 'FontSmoke', 'SceneGPUSmoke', 'EditorSmoke') and 'llvmpipe' not in log.read_text(errors='replace').lower():
                    raise RuntimeError(f'Expected software renderer evidence missing: {log}')
    print('PASS: actual Windows software WGL checks; this is not hardware or macOS validation')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
