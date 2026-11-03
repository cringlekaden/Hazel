"""Command-scoped full-Xcode selection for native producer operations."""
import json
import os
from pathlib import Path
import subprocess


def select(root):
    explicit = os.environ.get('HAZEL_DEVELOPER_DIR') or os.environ.get('DEVELOPER_DIR')
    selected = root / 'build/tools/macos-toolchain.json'
    candidates = []
    if explicit:
        candidates = [Path(explicit)]
    else:
        if selected.is_file():
            remembered = Path(json.loads(selected.read_text())['developer_directory'])
            if not (remembered / 'usr/bin/xcodebuild').is_file():
                raise RuntimeError('Remembered Xcode is unavailable: '+str(remembered)+'. Set HAZEL_DEVELOPER_DIR to the intended installation and regenerate.')
            candidates.append(remembered)
        probe = subprocess.run(['/usr/bin/xcode-select', '-p'], capture_output=True, text=True)
        if not probe.returncode: candidates.append(Path(probe.stdout.strip()))
        candidates += sorted(Path('/Applications').glob('Xcode*.app/Contents/Developer'))
        candidates += sorted(Path('/Volumes').glob('*/Applications/Xcode*.app/Contents/Developer'))
    directory = next((p for p in candidates if p.is_absolute() and (p / 'usr/bin/xcodebuild').is_file()), None)
    if directory is None:
        raise RuntimeError('Full Xcode is unavailable. Set HAZEL_DEVELOPER_DIR=/path/to/Xcode.app/Contents/Developer for native builds; script compilation only needs the prepared Mono SDK.')
    os.environ['DEVELOPER_DIR'] = str(directory)
    return directory


def remember(root, directory, mono_sdk):
    destination = root / 'build/tools/macos-toolchain.json'
    destination.parent.mkdir(parents=True, exist_ok=True)
    try: mono = str(mono_sdk.relative_to(root))
    except ValueError: mono = str(mono_sdk)
    destination.write_text(json.dumps({'developer_directory': str(directory), 'mono_sdk': mono}, indent=2) + '\n')
