-- Actual target managed example workspace, adapted for Linux and Windows.
-- Main repository generation also builds/deploys this assembly per configuration.
local root = path.getabsolute("../../../..", _MAIN_SCRIPT_DIR)
if os.target() == "linux" then _OPTIONS["dotnet"] = "mono" end
workspace "Sandbox"
    architecture "x64"
    configurations { "Debug", "Release", "Dist" }
for _, name in ipairs { "Sandbox", "Hazel-ScriptCore" } do
    project (name)
        kind "SharedLib"
        language "C#"
        dotnetframework "4.7.2"
        targetdir ("Binaries")
        objdir ("Intermediates/%{cfg.buildcfg}/" .. name)
        links { "System", "System.Core" }
        if name == "Sandbox" then
            files { "Source/**.cs", "Properties/**.cs" }
            links { "Hazel-ScriptCore" }
        else
            files { root .. "/Hazel-ScriptCore/Source/**.cs", root .. "/Hazel-ScriptCore/Properties/**.cs" }
        end
        filter "system:linux"
            buildoptions { "-sdk:4.7.2" }
        filter "configurations:Debug"
            optimize "Off"
            symbols "Default"
        filter "configurations:Release"
            optimize "On"
            symbols "Default"
        filter "configurations:Dist"
            optimize "Full"
            symbols "Off"
        filter {}
end
