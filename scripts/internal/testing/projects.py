"""Disposable committed example inputs; never compile or edit the owner's drafts."""
import io
from pathlib import Path
import subprocess
import tarfile
import hazel as hz


def snapshot(name, destination, descriptor=None):
    destination=Path(destination);destination.mkdir(parents=True,exist_ok=True)
    # Keep root .gitattributes in scope: subtree archives lose its exact-byte
    # rules and core.autocrlf can rewrite accepted assets on Windows.
    prefix='examples/'+name+'/'
    data=subprocess.check_output(['git','-C',str(hz.ROOT),'archive','HEAD',prefix[:-1]])
    with tarfile.open(fileobj=io.BytesIO(data)) as archive:
        members=[]
        for member in archive.getmembers():
            if not member.name.startswith(prefix):continue
            member.name=member.name[len(prefix):]
            if not member.name:continue
            if not (destination/member.name).resolve().is_relative_to(destination.resolve()) or member.issym() or member.islnk():
                raise RuntimeError('Unsafe committed example archive member: '+member.name)
            members.append(member)
        archive.extractall(destination,members=members,**({'filter':'data'} if hasattr(tarfile,'data_filter') else {}))
    if descriptor is not None:
        selected=destination/descriptor
        if selected.parent!=destination or not selected.is_file():raise RuntimeError('Missing explicit committed descriptor: '+str(descriptor))
        return selected
    descriptors=list(destination.glob('*.hproj'))
    if len(descriptors)!=1:raise RuntimeError('Expected one committed project: '+name)
    return descriptors[0]
