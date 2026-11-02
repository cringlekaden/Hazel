"""Focused shared-contract tests and optional isolated Xcode editor launch fixture."""
import argparse
import plistlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'scripts'))
from internal.target import host_target
from internal import macos_toolchain
from macos.schemes import generate

class Contracts(unittest.TestCase):
    def test_native_target_selection(self):
        targets=json.loads((ROOT/'scripts/internal/toolchain.json').read_text())['targets']
        for system,machine,name in [('Darwin','arm64','macos'),('Windows','AMD64','windows'),('Linux','x86_64','linux')]:
            self.assertEqual(host_target(targets,system,machine)[0],name)
        for system,machine in [('Darwin','x86_64'),('FreeBSD','x86_64'),('Linux','arm64')]:
            with self.assertRaises(RuntimeError):host_target(targets,system,machine)

    def test_invalid_explicit_xcode_does_not_fall_back(self):
        with patch.dict(os.environ,{'HAZEL_DEVELOPER_DIR':'/missing/explicit/Xcode'},clear=False):
            with self.assertRaises(RuntimeError):macos_toolchain.select(ROOT)

    def test_producer_selection_remembers_relocatable_owned_sdk(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            macos_toolchain.remember(root,Path('/chosen/Xcode.app/Contents/Developer'),root/'build/sdk space é')
            data=json.loads((root/'build/tools/macos-toolchain.json').read_text())
            self.assertEqual(data['mono_sdk'],'build/sdk space é')
            self.assertEqual(data['developer_directory'],'/chosen/Xcode.app/Contents/Developer')

    def test_scheme_regeneration_preserves_launch_configuration(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            for name,project in [('Hazelnut','build/Hazelnut/Hazelnut.xcodeproj'),('Nutella','Nutella/Nutella.xcodeproj')]:
                p=root/project;p.mkdir(parents=True)
                (p/'project.pbxproj').write_text('0123456789ABCDEF01234567 /* '+name+' */ = {\nisa = PBXNativeTarget;')
            generate(root)
            path=root/'Hazel.xcworkspace/xcshareddata/xcschemes/Hazelnut.xcscheme'
            scheme=ET.parse(path);launch=scheme.find('LaunchAction');launch.set('buildConfiguration','Release')
            launch.find('CommandLineArguments')[0].set('argument','owner choice é')
            scheme.write(path);generate(root)
            launch=ET.parse(path).find('LaunchAction')
            self.assertEqual(launch.get('buildConfiguration'),'Release')
            self.assertEqual(launch.find('CommandLineArguments')[0].get('argument'),'owner choice é')

def gui_fixture():
    parent=ROOT/'build/testing/macos';parent.mkdir(parents=True,exist_ok=True)
    private=Path(tempfile.mkdtemp(prefix='SDK GUI é ',dir=parent))
    project=private/'Project ü'
    subprocess.run([sys.executable,ROOT/'scripts/hazel.py','new-project','--name','SDK GUI Check','--identifier','SDKGUICheck','--destination',project],check=True)
    tree=ET.parse(ROOT/'Hazel.xcworkspace/xcshareddata/xcschemes/Hazelnut.xcscheme')
    launch=tree.find('LaunchAction');launch.set('buildConfiguration','Debug')
    # A test-only LaunchServices identity lets desktop inspection address the
    # bare development executable. The real binary/resources remain in bin/.
    wrapper=private/'SDK Verification.app'
    executable=wrapper/'Contents/MacOS/Hazelnut';executable.parent.mkdir(parents=True)
    executable.symlink_to(ROOT/'bin/Debug-macos-arm64/Hazelnut/Hazelnut')
    with (wrapper/'Contents/Info.plist').open('wb') as output:
        plistlib.dump({'CFBundleIdentifier':'com.kadencringle.hazel.sdkverification.'+private.name.split()[-1],'CFBundleName':'Hazel SDK Verification','CFBundleExecutable':'Hazelnut','CFBundlePackageType':'APPL','NSHighResolutionCapable':True},output)
    runnable=launch.find('BuildableProductRunnable')
    if runnable is not None:launch.remove(runnable)
    ET.SubElement(launch,'PathRunnable',runnableDebuggingMode='0',FilePath=str(executable))
    arguments=launch.find('CommandLineArguments');arguments.clear()
    ET.SubElement(arguments,'CommandLineArgument',argument='"'+str(project/'SDKGUICheck.hproj')+'"',isEnabled='YES')
    environment=launch.find('EnvironmentVariables')
    if environment is None:environment=ET.SubElement(launch,'EnvironmentVariables')
    environment.clear()
    for name,value in {'HAZEL_DATA':str(private/'profile'),'HAZEL_SAVE_ROOT':str(private/'saves'),'PATH':'/usr/bin:/bin'}.items():
        ET.SubElement(environment,'EnvironmentVariable',key=name,value=value,isEnabled='YES')
    destination=ROOT/'Hazel.xcworkspace/xcshareddata/xcschemes/Hazelnut SDK Verification.xcscheme'
    tree.write(destination,encoding='UTF-8',xml_declaration=True)
    print('Private Xcode scheme:',destination)
    print('Private project/profile:',private)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--gui-scheme',action='store_true');args=parser.parse_args()
    result=unittest.TextTestRunner().run(unittest.defaultTestLoader.loadTestsFromTestCase(Contracts))
    if not result.wasSuccessful():sys.exit(1)
    if args.gui_scheme:gui_fixture()
