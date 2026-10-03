"""Explicit starter project service shared by CLI and Hazelnut tool jobs."""
from pathlib import Path
import os
import re
import tempfile
import hazel as hz


def identifier(value):
    if not re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*', value) or value in {'out', 'virtual', 'try', 'short', 'Entity', 'string', 'throw', 'null', 'finally', 'new', 'operator', 'class', 'ref', 'foreach', 'namespace', 'delegate', 'char', 'override', 'const', 'in', 'continue', 'private', 'Hazel', 'extern', 'false', 'params', 'uint', 'volatile', 'long', 'do', 'default', 'sizeof', 'for', 'using', 'internal', 'break', 'struct', 'goto', 'stackalloc', 'bool', 'implicit', 'sealed', 'float', 'is', 'ushort', 'switch', 'sbyte', 'byte', 'abstract', 'explicit', 'double', 'unsafe', 'fixed', 'readonly', 'as', 'Prefab', 'static', 'void', 'unchecked', 'base', 'checked', 'enum', 'public', 'object', 'ulong', 'lock', 'while', 'catch', 'case', 'else', 'if', 'decimal', 'true', 'event', 'this', 'int', 'protected', 'return', 'interface', 'typeof'}:
        raise RuntimeError('Use a C# identifier: letters, digits and underscore, starting with a letter or underscore; avoid reserved names')
    return value


def publish_new_directory(source, destination):
    # Windows rename refuses an existing destination. Linux requires NOREPLACE
    # to avoid replacing a concurrently-created empty directory.
    if hz.SYSTEM=='windows': source.rename(destination); return
    import ctypes
    import errno
    libc=ctypes.CDLL(None,use_errno=True)
    rename=libc.renameat2
    rename.argtypes=[ctypes.c_int,ctypes.c_char_p,ctypes.c_int,ctypes.c_char_p,ctypes.c_uint]
    rename.restype=ctypes.c_int
    if rename(-100,os.fsencode(source),-100,os.fsencode(destination),1):
        code=ctypes.get_errno()
        if code==errno.EEXIST: raise FileExistsError('Destination appeared during creation; no existing directory was overwritten')
        raise OSError(code,os.strerror(code),str(destination))


def create_project(name, technical, destination):
    import yaml
    identifier(technical)
    if not name.strip() or len(name)>120: raise RuntimeError('Project display name must contain 1–120 characters')
    destination=destination.resolve()
    if destination.exists(): raise RuntimeError('Destination already exists. Choose a new project folder; no files were overwritten.')
    if not destination.parent.is_dir(): raise RuntimeError('Choose an existing destination parent directory')
    # Stage and build before publishing the project directory. Failure leaves the session/destination intact.
    with tempfile.TemporaryDirectory(prefix='.hazel-create-',dir=destination.parent) as temporary:
        root=Path(temporary)/'Project'; root.mkdir()
        for folder in ('Scenes','Scripts/Source','Scripts/Binaries','Textures','Prefabs'):
            (root/'Assets'/folder).mkdir(parents=True,exist_ok=True)
        config={'Version':1,'Name':name,'ScriptProject':technical,'StartScene':'Scenes/Start.hazel','AssetDirectory':'Assets','ScriptModulePath':f'Scripts/Binaries/{technical}.dll'}
        descriptor=root/(technical+'.hproj')
        descriptor.write_text(yaml.safe_dump({'Project':config},sort_keys=False,allow_unicode=True),encoding='utf-8')
        # Deliberate minimal template, never copied from an example game.
        camera={'ProjectionType':1,'PerspectiveFOV':.7853982,'PerspectiveNear':.01,'PerspectiveFar':1000,'OrthographicSize':10,'OrthographicNear':-1,'OrthographicFar':1}
        scene={'Scene':'Start','Entities':[{'Entity':1,'TagComponent':{'Tag':'Camera'},'TransformComponent':{'Translation':[0,0,0],'Rotation':[0,0,0],'Scale':[1,1,1]},'CameraComponent':{'Camera':camera,'Primary':True,'FixedAspectRatio':False}},
            {'Entity':2,'TagComponent':{'Tag':'Welcome'},'TransformComponent':{'Translation':[-3,0,0],'Rotation':[0,0,0],'Scale':[.5,.5,1]},'TextComponent':{'TextString':name,'Color':[1,1,1,1],'Kerning':0,'LineSpacing':0}}]}
        (root/'Assets/Scenes/Start.hazel').write_text(yaml.safe_dump(scene,sort_keys=False,allow_unicode=True),encoding='utf-8')
        template=(hz.ROOT/'scripts/internal/templates/project.lua').read_text(encoding='utf-8').replace('@IDENTIFIER@',technical)
        (root/'Assets/Scripts/premake5.lua').write_text(template,encoding='utf-8')
        (root/'Assets/Scripts/Source/Example.cs').write_text('''using Hazel;
namespace '''+technical+''' {
    // Assign this class in the entity Inspector after Build Scripts.
    public class Example : Entity {
        public Prefab Obstacle;
        private Entity spawned;
        void OnUpdate(float dt) {
            if (Input.IsKeyDown(KeyCode.Space) && spawned == null && Obstacle != null && Obstacle.IsAssigned)
                spawned = Entity.Instantiate(Obstacle, new Vector3(0, 0, 0));
            if (Input.IsKeyDown(KeyCode.R) && spawned != null) {
                spawned.Destroy(); // Invalid immediately; cleanup at the safe callback boundary.
                spawned = null;
            }
        }
        void OnDestroy() { if (spawned != null) spawned.Destroy(); }
    }
}
''',encoding='utf-8')
        hz.script_build(descriptor,'Debug')
        if destination.exists(): raise RuntimeError('Destination appeared during creation; choose another folder')
        publish_new_directory(root,destination)
    print('Created project:',destination/(technical+'.hproj'),flush=True)


def preflight(operation):
    if hz.SYSTEM=='windows':
        hz.vs_toolchain(operation=='export')
        targeting=Path(os.environ.get('ProgramFiles(x86)','C:/Program Files (x86)'))/'Reference Assemblies/Microsoft/Framework/.NETFramework/v4.7.2/mscorlib.dll'
        if not targeting.is_file(): raise RuntimeError('Install .NET Framework 4.7.2 targeting pack for script builds')
    else:
        prefix=hz.ROOT/'build/dependencies/mono/linux/usr'
        if not (prefix/'lib/mono/4.5/mcs.exe').is_file() and not Path('/usr/lib/mono/4.5/mcs.exe').is_file():
            raise RuntimeError('Mono compiler SDK unavailable. Run SDK setup once or install mono-devel; precompiled Play still works.')
    if not (hz.binaries('Debug')/'Hazel-ScriptCore/Hazel-ScriptCore.dll').is_file():
        raise RuntimeError('SDK Debug ScriptCore is missing. Build/setup this SDK first; editor creation/build needs matching ScriptCore.')
    premake=hz.ROOT/'build/tools/premake-core/bin/release'/('premake5.exe' if hz.SYSTEM=='windows' else 'premake5')
    if not premake.is_file(): raise RuntimeError('SDK Premake is missing. Run SDK setup before editor builds.')
    if operation=='export': hz.diagnose()
    print('READY: Python, configured SDK, Premake and script compiler'+(' and host native compiler' if operation=='export' else '')+' available.',flush=True)
