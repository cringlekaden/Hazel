"""Disposable committed example inputs; never compile or edit the owner's drafts."""
import io
from pathlib import Path
import subprocess
import tarfile
import hazel as hz


def snapshot(name, destination, descriptor=None):
    destination=Path(destination);destination.mkdir(parents=True,exist_ok=True)
    data=subprocess.check_output(['git','-C',str(hz.ROOT),'archive','HEAD:examples/'+name])
    with tarfile.open(fileobj=io.BytesIO(data)) as archive:
        for member in archive.getmembers():
            if not (destination/member.name).resolve().is_relative_to(destination.resolve()) or member.issym() or member.islnk():
                raise RuntimeError('Unsafe committed example archive member: '+member.name)
        archive.extractall(destination,**({'filter':'data'} if hasattr(tarfile,'data_filter') else {}))
    if descriptor is not None:
        selected=destination/descriptor
        if selected.parent!=destination or not selected.is_file():raise RuntimeError('Missing explicit committed descriptor: '+str(descriptor))
        return selected
    descriptors=list(destination.glob('*.hproj'))
    if len(descriptors)!=1:raise RuntimeError('Expected one committed project: '+name)
    return descriptors[0]
