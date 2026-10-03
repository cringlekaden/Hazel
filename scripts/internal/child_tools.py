"""Deterministic executable discovery for Python-backed authoring children."""
import os
from pathlib import Path
import shutil


def resolve(name):
    path=Path(name)
    if path.is_absolute():
        if not path.is_file(): raise RuntimeError('Missing tool executable: '+str(path))
        return path
    if len(path.parts)>1: raise RuntimeError('Tool executable must be absolute: '+str(path))
    candidates=[]
    if os.name=='nt':
        if name in ('cmd','cmd.exe'):
            candidates=[Path(os.environ.get('SystemRoot','C:/Windows'))/'System32/cmd.exe']
        if name in ('git','git.exe'):
            for key in ('ProgramFiles','ProgramFiles(x86)','LOCALAPPDATA'):
                root=Path(os.environ.get(key,''))
                if root.is_absolute(): candidates += [root/'Git/cmd/git.exe',root/'Programs/Git/cmd/git.exe']
    else:
        candidates=[Path(directory)/name for directory in ('/usr/bin','/bin','/usr/local/bin')]
    found=shutil.which(name)
    if found: candidates.append(Path(found).absolute())
    for candidate in candidates:
        if candidate.is_file() and (os.name=='nt' or os.access(candidate,os.X_OK)): return candidate.resolve()
    raise RuntimeError('Required tool '+name+' is unavailable. Configure/install the Hazel SDK prerequisites; no tools were installed automatically.')


def prepare(names):
    # Premake/Make invoke compiler helpers. Give them the directories of explicitly
    # resolved tools, rather than inheriting an accidentally complete interactive PATH.
    selected={name:resolve(name) for name in names}
    directories=list(dict.fromkeys(str(path.parent) for path in selected.values()))
    os.environ['PATH']=os.pathsep.join(directories+[os.environ.get('PATH','')])
    for name,path in selected.items():print('Tool:',name,'=',path,flush=True)
    return selected
