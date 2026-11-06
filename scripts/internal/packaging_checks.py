"""Focused native requirement/managed closure and archive-publication contracts."""
import hashlib
import os
import subprocess
from pathlib import Path
import tempfile

import hazel as hz


def content_only_contracts(profile):
    from internal.packaging import load_project, validate_project, package
    from internal.testing.projects import snapshot
    from internal.authoring import create_project
    from internal.package_tests import extract_verified
    hz.yaml_tools() # Reuse the checkout-owned pin in this fresh test-packages process.
    import yaml
    with tempfile.TemporaryDirectory(prefix='Hazel package contracts é ') as temp:
        working=Path(temp)
        descriptor=snapshot('RenderingReference',working/'reference','RenderingReference.hproj')
        assets=descriptor.parent/'Assets'
        module=assets/'Scripts/Binaries/RenderingReference.dll'
        assert not module.exists(), 'Content-only fixture unexpectedly has a compiled stub'
        output=working/'packages'
        package('Nutella',descriptor,output,'ContentOnly')
        archive=next(output.glob('ContentOnly-*'+('.zip' if hz.SYSTEM in ('windows','macos') else '.tar.gz')))
        accepted=hashlib.sha256(archive.read_bytes()).digest()
        checksum=archive.with_name(archive.name+'.sha256').read_bytes()

        scene=assets/'Scenes/Core_v1-Orthographic.hazel'
        document=yaml.safe_load(scene.read_text(encoding='utf-8'))
        def assign(name):
            document['Entities'][0]['ScriptComponent']={'ClassName':name}
            scene.write_text(yaml.safe_dump(document,sort_keys=False,allow_unicode=True),encoding='utf-8')
        def reject(needle):
            try:package('Nutella',descriptor,output,'ContentOnly')
            except RuntimeError as error:
                if needle.lower() not in str(error).lower():raise
            else:raise RuntimeError('Invalid required module/class/dependency was packaged')
            assert hashlib.sha256(archive.read_bytes()).digest()==accepted
            assert archive.with_name(archive.name+'.sha256').read_bytes()==checksum

        # Imported CPU resources are required even when their scene is not the
        # runtime start scene. Rejected replacements must preserve the archive.
        geometry=next(assets.rglob('*.hgeom'))
        saved_geometry=geometry.read_bytes()
        try:
            geometry.unlink();reject('')
            geometry.write_bytes(b'corrupt cooked geometry');reject('')
        finally:geometry.write_bytes(saved_geometry)
        image=next((assets/'Reference/Import_v1/Native').rglob('*.tga'))
        saved_image=image.read_bytes()
        try:
            image.unlink();reject('')
        finally:image.write_bytes(saved_image)

        assign('DependencyProbe.Behavior')
        reject('Assigned scripts require')
        module.parent.mkdir(parents=True,exist_ok=True);module.write_bytes(b'corrupt required assembly')
        reject('Package validation failed')

        # Use canonical compiler authoring for a real dependency edge. The
        # reference archive above was produced before any compiler invocation.
        dependency=create_project('Dependency fixture','RequiredDependency',working/'dependency')
        source=dependency.parent/'Assets/Scripts/Source/Example.cs'
        source.write_text('namespace RequiredDependency { public class Payload {} }\n',encoding='utf-8')
        hz.script_build(dependency,'Release')
        dependency_file=dependency.parent/'Assets/Scripts/Binaries/RequiredDependency.dll'
        staged_dependency=module.parent/'RequiredDependency.dll'
        hz.copy_changed(dependency_file,staged_dependency)
        sources=assets/'Scripts/Source'
        for file in sources.glob('*.cs'):file.unlink() # Disposable generated fixture only.
        (sources/'Behavior.cs').write_text('namespace DependencyProbe { public class Behavior : Hazel.Entity { public RequiredDependency.Payload Value; } }\n',encoding='utf-8')
        recipe=assets/'Scripts/premake5.lua'
        recipe.write_text(recipe.read_text(encoding='utf-8')+'\nlinks { "Binaries/RequiredDependency.dll" }\n',encoding='utf-8')
        hz.script_build(descriptor,'Release')
        selected,config,root=load_project(descriptor);validate_project(selected,config,root)
        staged_dependency.unlink()
        reject('Dependency is absent')
        hz.copy_changed(dependency_file,staged_dependency)
        assign('Unavailable.AssignedClass');reject('Unavailable compiled script class')
        assign('');module.write_bytes(b'corrupt present assembly')
        reject('Package validation failed')

        extracted=extract_verified(archive,working/'relocated')
        if hz.SYSTEM=='macos':
            from macos.qualification import product_acceptance
            logs=hz.ROOT/'build/testing/packages/macos';logs.mkdir(parents=True,exist_ok=True)
            cwd=working/'unrelated cwd';cwd.mkdir()
            product_acceptance('Nutella',extracted/'Nutella.app',cwd,logs,hz.mono_prefix())
        else:
            from internal.package_tests import graphics_environment
            if hz.SYSTEM=='windows' and profile!='native':
                from internal.tests import software_driver
                for dll in software_driver().glob('*.dll'):hz.copy_changed(dll,extracted/dll.name)
            env=graphics_environment(profile)
            env.update(HAZEL_DATA=str(working/'player-profile'),HAZEL_SAVE_ROOT=str(working/'player-saves'))
            cwd=working/'unrelated cwd';cwd.mkdir()
            result=subprocess.run([str(extracted/('Nutella'+hz.EXECUTABLE_SUFFIX)),'--smoke-test'],cwd=cwd,env=env,capture_output=True,text=True,encoding='utf-8',timeout=120)
            logs=hz.ROOT/'build/testing/packages';logs.mkdir(parents=True,exist_ok=True)
            (logs/(hz.SYSTEM+'-content-only-player.log')).write_text(result.stdout+result.stderr,encoding='utf-8')
            if result.returncode or 'SMOKE: 60 rendered frames' not in result.stdout or 'SMOKE PASS: application teardown completed' not in result.stdout:
                raise RuntimeError('Relocated content-only player failed: '+result.stdout+result.stderr)
        print('PASS: native content-only packaging without a script stub, assigned module/class and managed dependency rejection, accepted archive preservation'+
              ('; source/SDK-denied relocated content-only player' if hz.SYSTEM=='macos' else ''),flush=True)
