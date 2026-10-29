"""Generate shared launch schemes from Premake's generated target identities."""
import re
import xml.etree.ElementTree as ET

def generate(root):
    directory=root/'Hazel.xcworkspace/xcshareddata/xcschemes'
    directory.mkdir(parents=True,exist_ok=True)
    for name,project in [('Hazelnut','build/Hazelnut/Hazelnut.xcodeproj'),('Nutella','Nutella/Nutella.xcodeproj')]:
        text=(root/project/'project.pbxproj').read_text()
        target=re.search(r'([A-F0-9]{24}) /\* '+name+r' \*/ = \{\s*isa = PBXNativeTarget;',text)
        if not target: raise RuntimeError('Missing generated Xcode target: '+name)
        scheme=ET.Element('Scheme',LastUpgradeVersion='2700',version='1.3')
        build=ET.SubElement(scheme,'BuildAction',parallelizeBuildables='YES',buildImplicitDependencies='YES')
        entry=ET.SubElement(ET.SubElement(build,'BuildActionEntries'),'BuildActionEntry',buildForTesting='YES',buildForRunning='YES',buildForProfiling='YES',buildForArchiving='YES',buildForAnalyzing='YES')
        def reference(parent):
            ET.SubElement(parent,'BuildableReference',BuildableIdentifier='primary',BlueprintIdentifier=target[1],BuildableName=name,BlueprintName=name,ReferencedContainer='container:'+project)
        reference(entry)
        launch=ET.SubElement(scheme,'LaunchAction',buildConfiguration='Debug',selectedDebuggerIdentifier='Xcode.DebuggerFoundation.Debugger.LLDB',selectedLauncherIdentifier='Xcode.IDEFoundation.Launcher.LLDB',useCustomWorkingDirectory='NO',launchStyle='0',debugServiceExtension='internal',allowLocationSimulation='NO')
        reference(ET.SubElement(launch,'BuildableProductRunnable',runnableDebuggingMode='0'))
        arguments=ET.SubElement(launch,'CommandLineArguments')
        # Xcode expands SRCROOT at launch; no personal checkout path is stored.
        if name=='Nutella': ET.SubElement(arguments,'CommandLineArgument',argument='--project',isEnabled='YES')
        relative='../../examples/SceneTransitions/SceneTransitions.hproj' if name=='Hazelnut' else '../examples/SceneTransitions/SceneTransitions.hproj'
        ET.SubElement(arguments,'CommandLineArgument',argument='$(SRCROOT)/'+relative,isEnabled='YES')
        ET.SubElement(scheme,'AnalyzeAction',buildConfiguration='Debug')
        ET.SubElement(scheme,'ArchiveAction',buildConfiguration='Release',revealArchiveInOrganizer='YES')
        ET.indent(scheme)
        ET.ElementTree(scheme).write(directory/(name+'.xcscheme'),encoding='UTF-8',xml_declaration=True)
