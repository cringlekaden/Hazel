-- macOS integration stays root-owned; generated Xcode files are disposable.
local root = _MAIN_SCRIPT_DIR
local mono = path.getabsolute(assert(_OPTIONS["mono-root"], "Generate through scripts/hazel.py"))
if _OPTIONS["migration-tests"] then
    project "MacOSPlatformServices"
        location (root .. "/build/MacOSPlatformServices")
        kind "ConsoleApp"
        language "C++"
        cppdialect "C++17"
        targetdir (root .. "/bin/" .. outputdir .. "/%{prj.name}")
        objdir (root .. "/bin-int/" .. outputdir .. "/%{prj.name}")
        files { root .. "/tests/macos/PlatformServices.cpp", root .. "/Hazel/src/Platform/macOS/MacOSProcess.cpp", root .. "/Hazel/src/Platform/macOS/MacOSFileSystem.cpp", root .. "/Hazel/src/Platform/macOS/MacOSFileLease.cpp" }
        includedirs { root .. "/Hazel/src", root .. "/Hazel/vendor/spdlog/include" }
end
project "freetype"
    defines { "FT_CONFIG_OPTION_SYSTEM_ZLIB" }
for _, name in ipairs { "Hazel-ScriptCore", "PackageAudit" } do
    project (name)
        kind "Utility"
        language "C++"
        prebuildcommands { '"' .. assert(os.getenv("HAZEL_PYTHON"), "Generate through scripts/hazel.py") .. '" "' .. root .. '/scripts/macos/managed.py" %{cfg.buildcfg} ' .. name }
end
project "Hazel"
    xcodebuildsettings { ["GCC_PREFIX_HEADER"] = root .. "/Hazel/src/hzpch.h" }
    removefiles { root .. "/Hazel/src/Platform/Windows/**", root .. "/Hazel/src/Platform/Linux/**" }
    defines { "HZ_PLATFORM_MACOS", "GL_SILENCE_DEPRECATION" }
    filter "files:**.mm"
        enablepch "Off"
    filter {}
project "Hazelnut"
    dependson { "HazelProject", "PackageAudit" }
local consumers = { "Nutella", "Hazelnut", "SpriteAssetAudit", "HazelProject" }
if _OPTIONS["migration-tests"] then
    for _, name in ipairs { "Renderer2DSmoke", "FontSmoke", "RendererFeaturesSmoke" } do table.insert(consumers, "Migration" .. name) end
end
for _, name in ipairs(consumers) do
    project (name)
        links { "Cocoa.framework", "IOKit.framework", "CoreFoundation.framework", "OpenGL.framework", "CoreAudio.framework", "AudioToolbox.framework" }
        defines { "HZ_PLATFORM_MACOS", "GL_SILENCE_DEPRECATION" }
        libdirs { mono .. "/lib" }
        links { "monosgen-2.0", "z" }
        xcodebuildsettings { ["LD_RUNPATH_SEARCH_PATHS"] = "@executable_path/mono/lib" }
        if name == "Hazelnut" or name == "Nutella" then
            dependson { "Hazel-ScriptCore" }
            postbuildcommands { '"' .. os.getenv("HAZEL_PYTHON") .. '" "' .. root .. '/scripts/macos/stage.py" %{cfg.buildcfg}' }
        end
end
