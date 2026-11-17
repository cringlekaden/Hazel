-- macOS integration stays root-owned; generated Xcode files are disposable.
local root = _MAIN_SCRIPT_DIR
local mono = path.getabsolute(assert(_OPTIONS["mono-root"], "Generate through scripts/hazel.py"))
local tests = json.decode(io.readfile(root .. "/scripts/internal/testing/native-tests.json"))
if _OPTIONS["migration-tests"] then
    project "MacOSSDKIntegration"
        location (root .. "/build/MacOSSDKIntegration")
        kind "ConsoleApp"
        language "C++"
        cppdialect "C++17"
        targetdir (root .. "/bin/" .. outputdir .. "/%{prj.name}")
        objdir (root .. "/bin-int/" .. outputdir .. "/%{prj.name}")
        files { root .. "/tests/macos/SDKIntegration.cpp", root .. "/Hazelnut/src/Authoring/HazelSDK.cpp", root .. "/Hazelnut/src/Authoring/ProjectTools.cpp", root .. "/Hazelnut/src/Authoring/ConsoleModel.cpp", root .. "/Hazelnut/src/Authoring/EditorPreferences.cpp" }
        includedirs { root .. "/Hazel/src", root .. "/Hazelnut/src", root .. "/Hazel/vendor/spdlog/include", root .. "/Hazel/vendor/glm", root .. "/Hazel/vendor/yaml-cpp/include" }
        local pins = json.decode(io.readfile(root .. "/scripts/internal/toolchain.json"))
        defines { 'HAZEL_TOOLCHAIN_PREMAKE="' .. pins.premake .. '"', 'HAZEL_TOOLCHAIN_PYYAML="' .. pins.pyyaml .. '"' }
        links { "Hazel", "yaml-cpp" }
        dependson { "Hazelnut" }
    project "MacOSPlatformServices"
        location (root .. "/build/MacOSPlatformServices")
        kind "ConsoleApp"
        language "C++"
        cppdialect "C++17"
        targetdir (root .. "/bin/" .. outputdir .. "/%{prj.name}")
        objdir (root .. "/bin-int/" .. outputdir .. "/%{prj.name}")
        files { root .. "/tests/macos/PlatformServices.cpp" }
        includedirs { root .. "/Hazel/src", root .. "/Hazel/vendor/spdlog/include" }
        links { "Hazel" }
end
if _OPTIONS["migration-tests"] then
    project "MetalM1Smoke"
        location (root .. "/build/MetalM1Smoke")
        kind "ConsoleApp"
        language "C++"
        cppdialect "C++17"
        warnings "Extra"
        targetdir (root .. "/bin/" .. outputdir .. "/%{prj.name}")
        objdir (root .. "/bin-int/" .. outputdir .. "/%{prj.name}")
        files { root .. "/tests/macos/MetalM1Smoke.cpp", root .. "/tests/macos/MetalM1Environment.mm" }
        links { "ApplicationServices.framework" }
        includedirs { root .. "/Hazel/src" }
        externalincludedirs { root .. "/Hazel/vendor/metal-cpp", root .. "/Hazel/vendor/entt/include", root .. "/Hazel/vendor/yaml-cpp/include", root .. "/Hazel/vendor/spdlog/include", root .. "/Hazel/vendor/glm", root .. "/Hazel/vendor/imgui", root .. "/Hazel/vendor/GLFW/include", root .. "/Hazel/vendor/Glad/include" }
        libdirs { ShaderToolsRoot .. "/lib" }
        links { "Hazel" }
        links (ShaderToolsLinks)
        defines { "GLFW_INCLUDE_NONE" }
        filter "configurations:Debug"
            symbols "On"
        filter "configurations:Release"
            optimize "On"
        filter {}
    project "MetalM2ASmoke"
        location (root .. "/build/MetalM2ASmoke")
        kind "ConsoleApp"
        language "C++"
        cppdialect "C++17"
        warnings "Extra"
        targetdir (root .. "/bin/" .. outputdir .. "/%{prj.name}")
        objdir (root .. "/bin-int/" .. outputdir .. "/%{prj.name}")
        files { root .. "/tests/macos/MetalM2ASmoke.cpp", root .. "/tests/macos/MetalM2AShaders.cpp" }
        links { "ApplicationServices.framework" }
        includedirs { root .. "/Hazel/src" }
        externalincludedirs { ShaderToolsIncludes, root .. "/Hazel/vendor/metal-cpp" }
        buildoptions { "-fblocks" }
        links { "spirv-cross-msl" }
        externalincludedirs { root .. "/Hazel/vendor/spdlog/include", root .. "/Hazel/vendor/glm", root .. "/Hazel/vendor/imgui", root .. "/Hazel/vendor/GLFW/include", root .. "/Hazel/vendor/Glad/include" }
        libdirs { ShaderToolsRoot .. "/lib" }
        links { "Hazel" }
        links (ShaderToolsLinks)
        defines { "GLFW_INCLUDE_NONE" }
        filter "configurations:Debug"
            symbols "On"
        filter "configurations:Release"
            optimize "On"
        filter {}
    project "MetalShaderTools"
        location (root .. "/build/MetalShaderTools")
        kind "ConsoleApp"
        language "C++"
        cppdialect "C++17"
        warnings "Extra"
        targetdir (root .. "/bin/" .. outputdir .. "/%{prj.name}")
        objdir (root .. "/bin-int/" .. outputdir .. "/%{prj.name}")
        files { root .. "/tools/MetalShaderTools/MetalShaderTools.cpp" }
        links { "ApplicationServices.framework" }
        includedirs { root .. "/Hazel/src" }
        externalincludedirs { ShaderToolsIncludes, root .. "/Hazel/vendor/metal-cpp" }
        buildoptions { "-fblocks" }
        links { "spirv-cross-msl" }
        externalincludedirs { root .. "/Hazel/vendor/spdlog/include", root .. "/Hazel/vendor/glm", root .. "/Hazel/vendor/imgui", root .. "/Hazel/vendor/GLFW/include", root .. "/Hazel/vendor/Glad/include" }
        libdirs { ShaderToolsRoot .. "/lib" }
        links { "Hazel" }
        links (ShaderToolsLinks)
        defines { "GLFW_INCLUDE_NONE" }
        filter "configurations:Debug"
            symbols "On"
        filter "configurations:Release"
            optimize "On"
        filter {}
    project "MetalShaderSmoke"
        location (root .. "/build/MetalShaderSmoke")
        kind "ConsoleApp"
        language "C++"
        cppdialect "C++17"
        warnings "Extra"
        targetdir (root .. "/bin/" .. outputdir .. "/%{prj.name}")
        objdir (root .. "/bin-int/" .. outputdir .. "/%{prj.name}")
        files { root .. "/tests/macos/MetalShaderSmoke.cpp" }
        links { "ApplicationServices.framework" }
        includedirs { root .. "/Hazel/src" }
        externalincludedirs { ShaderToolsIncludes, root .. "/Hazel/vendor/metal-cpp" }
        buildoptions { "-fblocks" }
        links { "spirv-cross-msl" }
        externalincludedirs { root .. "/Hazel/vendor/spdlog/include", root .. "/Hazel/vendor/glm", root .. "/Hazel/vendor/imgui", root .. "/Hazel/vendor/GLFW/include", root .. "/Hazel/vendor/Glad/include" }
        libdirs { ShaderToolsRoot .. "/lib" }
        links { "Hazel" }
        links (ShaderToolsLinks)
        defines { "GLFW_INCLUDE_NONE" }
        filter "configurations:Debug"
            symbols "On"
        filter "configurations:Release"
            optimize "On"
        filter {}
    project "MetalRenderer2DQuadSmoke"
        location (root .. "/build/MetalRenderer2DQuadSmoke")
        kind "ConsoleApp"
        language "C++"
        cppdialect "C++17"
        warnings "Extra"
        targetdir (root .. "/bin/" .. outputdir .. "/%{prj.name}")
        objdir (root .. "/bin-int/" .. outputdir .. "/%{prj.name}")
        files { root .. "/tests/macos/MetalRenderer2DQuadSmoke.cpp" }
        links { "ApplicationServices.framework" }
        includedirs { root .. "/Hazel/src" }
        externalincludedirs { root .. "/Hazel/vendor/yaml-cpp/include", ShaderToolsIncludes, root .. "/Hazel/vendor/metal-cpp" }
        buildoptions { "-fblocks" }
        links { "spirv-cross-msl" }
        externalincludedirs { root .. "/Hazel/vendor/entt/include", root .. "/Hazel/vendor/yaml-cpp/include", root .. "/Hazel/vendor/spdlog/include", root .. "/Hazel/vendor/glm", root .. "/Hazel/vendor/imgui", root .. "/Hazel/vendor/GLFW/include", root .. "/Hazel/vendor/Glad/include" }
        libdirs { ShaderToolsRoot .. "/lib" }
        links { "Hazel" }
        links (ShaderToolsLinks)
        defines { "GLFW_INCLUDE_NONE" }
        filter "configurations:Debug"
            symbols "On"
        filter "configurations:Release"
            optimize "On"
        filter {}
end
project "freetype"
    defines { "FT_CONFIG_OPTION_SYSTEM_ZLIB" }
local managed = { "Hazel-ScriptCore", "PackageAudit" }
if _OPTIONS["migration-tests"] then table.insert(managed, "MigrationManagedFixture") end
for _, name in ipairs(managed) do
    project (name)
        kind "Utility"
        language "C++"
        prebuildcommands { '"' .. assert(os.getenv("HAZEL_PYTHON"), "Generate through scripts/hazel.py") .. '" "' .. root .. '/scripts/macos/managed.py" %{cfg.buildcfg} ' .. name }
end
if _OPTIONS["migration-tests"] then
    project "MigrationManagedFixture"
        dependson { "Hazel-ScriptCore" }
end
project "Hazel"
    -- The pinned Xcode generator omits file-filtered buildoptions. Use the
    -- native target setting so every Objective-C++ bridge has explicit ARC.
    xcodebuildsettings { ["GCC_PREFIX_HEADER"] = root .. "/Hazel/src/hzpch.h", ["CLANG_ENABLE_OBJC_ARC"] = "YES" }
    removefiles { root .. "/Hazel/src/Platform/Windows/**", root .. "/Hazel/src/Platform/Linux/**" }
    defines { "HZ_PLATFORM_MACOS", "GL_SILENCE_DEPRECATION" }
    externalincludedirs { root .. "/Hazel/vendor/metal-cpp" }
    buildoptions { "-fblocks" } -- Metal-cpp completion/presentation callbacks
    filter "files:**.mm"
        enablepch "Off"
    filter {}
project "Hazelnut"
    dependson { "HazelProject", "PackageAudit" }
local consumers = { "Nutella", "Hazelnut", "SpriteAssetAudit", "HazelProject" }
if _OPTIONS["migration-tests"] then
    for _, test in ipairs(tests) do if test.name ~= "ImportDependencySmoke" then table.insert(consumers, "Migration" .. test.name) end end
    table.insert(consumers, "MigrationExampleGamesSmoke")
    table.insert(consumers, "MetalM1Smoke")
    table.insert(consumers, "MetalM2ASmoke")
    table.insert(consumers, "MetalShaderTools")
    table.insert(consumers, "MetalShaderSmoke")
    table.insert(consumers, "MetalRenderer2DQuadSmoke")
end
for _, name in ipairs(consumers) do
    project (name)
        links { "spirv-cross-msl" }
        externalincludedirs { mono .. "/include/mono-2.0" }
        links { "ImGui", "GLFW", "Glad", "ImGuizmo", "yaml-cpp", "Box2D", "msdf-atlas-gen", "msdfgen", "freetype", "HarfBuzz", "UniBreak", "SheenBidi" }
        links { "Metal.framework", "QuartzCore.framework", "Foundation.framework", "Cocoa.framework", "IOKit.framework", "CoreFoundation.framework", "OpenGL.framework", "CoreAudio.framework", "AudioToolbox.framework" }
        defines { "HZ_PLATFORM_MACOS", "GL_SILENCE_DEPRECATION" }
        libdirs { ShaderToolsRoot .. "/lib", mono .. "/lib" }
        links { "monosgen-2.0", "z" }
        xcodebuildsettings { ["LD_RUNPATH_SEARCH_PATHS"] = "@executable_path/mono/lib" }
        if name == "Hazelnut" or name == "Nutella" or name == "HazelProject" or name == "SpriteAssetAudit" then
            dependson { "Hazel-ScriptCore" }
            postbuildcommands { '"' .. os.getenv("HAZEL_PYTHON") .. '" "' .. root .. '/scripts/macos/stage.py" %{cfg.buildcfg} ' .. name }
        end
end
if _OPTIONS["migration-tests"] then
    project "MacOSQualification"
        location (root .. "/build/MacOSQualification")
        kind "Utility"
        language "C++"
        local required = { "Hazelnut", "Nutella", "SpriteAssetAudit", "MigrationManagedFixture", "MigrationExampleGamesSmoke", "MacOSSDKIntegration", "MacOSPlatformServices", "MetalM1Smoke" }
        for _, test in ipairs(tests) do table.insert(required, "Migration" .. test.name) end
        dependson (required)
end
