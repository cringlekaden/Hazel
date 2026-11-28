"""Disposable committed example inputs; never compile or edit the owner's drafts."""
import io
import os
from pathlib import Path
import subprocess
import tarfile
import hazel as hz


def snapshot(name, destination, descriptor=None):
    destination=Path(destination);destination.mkdir(parents=True,exist_ok=True)
    # Keep root .gitattributes in scope: subtree archives lose its exact-byte
    # rules and core.autocrlf can rewrite accepted assets on Windows.
    prefix='examples/'+name+'/'
    revision=os.environ.get('HAZEL_TEST_EXAMPLE_REVISION','HEAD')
    commit=subprocess.check_output(['git','-C',str(hz.ROOT),'rev-parse','--verify',revision+'^{commit}'],text=True).strip()
    print('Example snapshot:',name,commit,flush=True)
    data=subprocess.check_output(['git','-C',str(hz.ROOT),'archive',commit,prefix[:-1]])
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


def snapshot_studio(destination):
    """Select the Studio material fixture in current and historical snapshots."""
    main = snapshot('RenderingShowcase', destination, 'RenderingShowcase.hproj')
    studio = main.with_name('RenderingShowcase-Studio.hproj')
    if studio.is_file():
        return studio
    if '  StartScene: Scenes/Gallery.hazel' in main.read_text(encoding='utf-8').splitlines():
        return main
    raise RuntimeError('The committed RenderingShowcase snapshot is missing its Studio fixture')
