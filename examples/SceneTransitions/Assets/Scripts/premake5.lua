-- Standalone authored-project build. ScriptCore is supplied by the Hazel SDK tool.
if os.target() == "linux" then _OPTIONS["dotnet"] = "mono" end
local core = os.getenv("HAZEL_SCRIPTCORE")
if not core or not os.isfile(core) then error("Use scripts/hazel.py script-build with a built Hazel SDK") end
workspace "SceneTransitions"
    architecture "x64"
    configurations { "Debug", "Release" }
project "SceneTransitions"
    kind "SharedLib"
    language "C#"
    dotnetframework "4.7.2"
    targetdir "Binaries"
    objdir "Intermediates/%{cfg.buildcfg}"
    files { "Source/**.cs" }
    links { core, "System", "System.Core" }
    filter "system:linux"
        buildoptions { "-sdk:4.7.2" }
    filter "configurations:Debug"
        symbols "Default"
    filter "configurations:Release"
        optimize "On"
        symbols "Off"
    filter {}
