-- Actual target Windows Mono SDK; Linux uses the native Mono development SDK.
newoption { trigger = "mono-root", value = "PATH", description = "Mono SDK prefix containing include/mono-2.0 and lib (Linux)" }
local root = _MAIN_SCRIPT_DIR
local windows = os.target() == "windows"
if os.target() == "linux" and not _OPTIONS["dotnet"] then _OPTIONS["dotnet"] = "mono" end
local prefix = _OPTIONS["mono-root"] and path.getabsolute(_OPTIONS["mono-root"], root) or "/usr"
local includes = windows and (root .. "/Hazel/vendor/mono/include") or (prefix .. "/include/mono-2.0")
local assemblies = windows and (root .. "/Hazelnut/mono/lib") or (prefix .. "/lib")
local config = windows and "" or path.getabsolute("../etc/mono/config", prefix)
if not windows and not os.isfile(includes .. "/mono/jit/jit.h") then
    error("Mono development headers missing: install mono-devel/mono or provide --mono-root=SDK_PREFIX")
end
include (root .. "/Hazel-ScriptCore")
project "SandboxScripts"
    location (root .. "/build/SandboxScripts")
    kind "SharedLib"
    language "C#"
    dotnetframework "4.7.2"
    targetname "Sandbox"
    targetdir (root .. "/bin/" .. outputdir .. "/SandboxScripts")
    objdir (root .. "/bin-int/" .. outputdir .. "/SandboxScripts")
    files { root .. "/Hazelnut/SandboxProject/Assets/Scripts/Source/**.cs" }
    links { "Hazel-ScriptCore", "System", "System.Core" }
    local scriptOutput = os.target() == "windows" and _ACTION:match("^vs") and "$(TargetPath)" or "%{cfg.buildtarget.abspath}"
    postbuildcommands { '{MKDIR} "' .. root .. '/Hazelnut/SandboxProject/Assets/Scripts/Binaries"',
        '{COPYFILE} "' .. scriptOutput .. '" "' .. root .. '/Hazelnut/SandboxProject/Assets/Scripts/Binaries/Sandbox.dll"' }
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

local consumers = { "Hazel", "Sandbox", "Hazelnut" }
if _OPTIONS["migration-tests"] then
    for _, test in ipairs { "RendererSmoke", "CoreSmoke", "RendererFeaturesSmoke", "SceneFoundationSmoke", "FontSmoke", "Renderer2DSmoke", "ProjectPhysicsSmoke", "MonoSmoke", "SceneSmoke", "SceneGPUSmoke", "EditorSmoke" } do
        table.insert(consumers, "Migration" .. test)
    end
end
for _, name in ipairs(consumers) do
    project (name)
        externalincludedirs { includes }
        if name == "MigrationMonoSmoke" then
            defines { 'HZ_MONO_ASSEMBLIES_PATH="' .. assemblies .. '"', 'HZ_MONO_CONFIG_PATH="' .. config .. '"' }
        end
        if name ~= "Hazel" then
            filter "system:linux"
                libdirs { prefix .. "/lib" }
                links { "monosgen-2.0", "pthread", "dl", "m" }
                runpathdirs { "$$ORIGIN/lib" }
                linkoptions { "-Wl,--disable-new-dtags" }
            filter "system:windows"
                links { "libmono-static-sgen", "Ws2_32", "Winmm", "Version", "Bcrypt", "Psapi", "Shell32", "Ole32" }
            filter { "system:windows", "configurations:Debug" }
                libdirs { root .. "/Hazel/vendor/mono/lib/Debug" }
            filter { "system:windows", "configurations:Release or Dist" }
                libdirs { root .. "/Hazel/vendor/mono/lib/Release" }
            filter {}
        end
end
if _OPTIONS["migration-tests"] then
    project "MigrationManagedFixture"
        location (root .. "/build/MigrationManagedFixture")
        kind "SharedLib"
        language "C#"
        dotnetframework "4.7.2"
        targetdir (root .. "/bin/" .. outputdir .. "/%{prj.name}")
        objdir (root .. "/bin-int/" .. outputdir .. "/%{prj.name}")
        files { root .. "/tests/migration/ManagedFixture.cs", root .. "/Hazelnut/SandboxProject/Assets/Scripts/Source/**.cs" }
        links { "Hazel-ScriptCore", "System", "System.Core" }
        filter "system:linux"
            buildoptions { "-sdk:4.7.2" }
        filter {}
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
    project "MigrationMonoSmoke"
        dependson { "Hazel-ScriptCore", "MigrationManagedFixture" }
    project "MigrationSceneSmoke"
        dependson { "Hazel-ScriptCore", "MigrationManagedFixture" }
    project "MigrationSceneGPUSmoke"
        dependson { "Hazel-ScriptCore", "MigrationManagedFixture" }
end
