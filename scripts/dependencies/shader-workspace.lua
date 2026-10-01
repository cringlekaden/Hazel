-- Complete pinned shader compiler workspace. Source manifest derives from the
-- dependencies' actual source sets; no dependency build requires CMake.
local root = path.getabsolute("../..", _MAIN_SCRIPT_DIR)
local source = root .. "/build/dependencies/sources"
local shaderc = source .. "/shaderc"
local glslang = shaderc .. "/third_party/glslang"
local tools = shaderc .. "/third_party/spirv-tools"
local headers = shaderc .. "/third_party/spirv-headers/include"
local generated = root .. "/build/dependencies/generated"
local output = root .. "/build/dependencies/premake"
local inputs = json.decode(io.readfile(root .. "/scripts/dependencies/shader-sources.json"))

workspace "HazelShaderTools"
    location (output)
    architecture "x64"
    configurations { "Debug", "Release" }

for _, name in ipairs { "shaderc", "glslang", "SPIRV-Tools", "SPIRV-Tools-opt", "spirv-cross-core", "spirv-cross-glsl" } do
    project (name)
        location (output .. "/" .. name)
        kind "StaticLib"
        language "C++"
        cppdialect "C++17"
        staticruntime "Off"
        warnings "Off"
        targetdir (output .. "/bin/%{cfg.buildcfg}-%{cfg.system}-x86_64")
        objdir (output .. "/obj/%{cfg.buildcfg}-%{cfg.system}-x86_64/" .. name)
        local sources = {}
        for _, file in ipairs(inputs[name]) do
            if name == "glslang" and os.target() == "windows" then
                file = file:gsub("OSDependent/Unix/ossource.cpp", "OSDependent/Windows/ossource.cpp")
            end
            table.insert(sources, source .. "/" .. file)
        end
        files (sources)
        if name == "shaderc" then
            includedirs { shaderc .. "/libshaderc/include", shaderc .. "/libshaderc_util/include",
                glslang, tools .. "/include", headers, generated .. "/include" }
            defines { "ENABLE_HLSL" }
        elseif name == "glslang" then
            includedirs { glslang, glslang .. "/External", tools, tools .. "/include", headers, generated .. "/include" }
            defines { "ENABLE_HLSL", "ENABLE_OPT=1" }
            exceptionhandling "Off"
            rtti "Off"
            filter "system:linux"
                defines { "GLSLANG_OSINCLUDE_UNIX" }
                forceincludes { root .. "/scripts/dependencies/shader-tools-compat.h" }
            filter "system:windows"
                defines { "GLSLANG_OSINCLUDE_WIN32" }
                buildoptions { '/FI"' .. root .. '/scripts/dependencies/shader-tools-compat.h"' }
            filter {}
        elseif name == "SPIRV-Tools" or name == "SPIRV-Tools-opt" then
            includedirs { tools, tools .. "/include", headers, generated .. "/spirv-tools" }
            defines { "SPIRV_COLOR_TERMINAL", "SPIRV_TIMER_ENABLED", "SPIRV_CHECK_CONTEXT" }
            exceptionhandling "Off"
            if name == "SPIRV-Tools" then rtti "Off" end
            filter "system:linux"
                defines { "SPIRV_LINUX" }
            filter "system:windows"
                defines { "SPIRV_WINDOWS" }
            filter {}
        else
            includedirs { source .. "/spirv-cross" }
        end
        filter "system:linux"
            toolset "gcc"
            pic "On"
        filter "system:windows"
            systemversion "latest"
            defines { "NOMINMAX", "_CRT_SECURE_NO_WARNINGS", "_SCL_SECURE_NO_WARNINGS" }
            buildoptions { "/utf-8" }
        filter "configurations:Debug"
            runtime "Debug"
            symbols "On"
        filter "configurations:Release"
            runtime "Release"
            optimize "Speed"
        filter {}
end
