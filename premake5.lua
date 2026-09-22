--
-- Hazel
-- Cross-platform Premake configuration
--
-- Supported:
--   Windows x86_64 - Visual Studio / MSVC
--   Linux   x86_64 - GNU Make / GCC
--
-- Projects:
--   GLFW    - static dependency
--   Hazel   - shared engine library
--   Sandbox - executable using Hazel
--

local workspaceRoot = path.getabsolute(".")
local imguiDir = workspaceRoot .. "/Hazel/vendor/imgui"
local glfwDir = workspaceRoot .. "/Hazel/vendor/GLFW"


workspace "Hazel"
    architecture "x64"
    startproject "Sandbox"

    configurations
    {
        "Debug",
        "Release",
        "Dist"
    }


--
-- Examples:
--
-- Debug-windows-x86_64
-- Debug-linux-x86_64
--
outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"


--
-- Absolute output locations.
--
-- These prevent generated project files inside Hazel/ and Sandbox/
-- from incorrectly rebasing paths through ../ or ../../.
--
local binRoot    = workspaceRoot .. "/bin/" .. outputdir
local binIntRoot = workspaceRoot .. "/bin-int/" .. outputdir



--
-- Compatibility for older vendor Premake scripts.
--
-- Current Premake removed the legacy flags() API in favor of
-- dedicated settings. TheCherno's GLFW Premake file still uses
-- NoRuntimeChecks and NoIncrementalLink for its Debug-AS config.
--
if flags == nil then
    function flags(values)
        for _, value in ipairs(values) do
            if value == "NoRuntimeChecks" then
                runtimechecks "Off"

            elseif value == "NoIncrementalLink" then
                incrementallink "Off"

            else
                error(
                    "Unsupported legacy Premake flag in vendor script: "
                    .. tostring(value)
                )
            end
        end
    end
end



-- ================================================================
-- Dependencies
-- ================================================================

group "Dependencies"
    include "Hazel/vendor/GLFW"
    include "Hazel/vendor/Glad"
group ""



--
-- Extend the vendor ImGui project with needed backends.
--
project "ImGui"

    location "build/ImGui"

    kind "StaticLib"
    language "C++"
    cppdialect "C++17"

    targetdir (binRoot .. "/%{prj.name}")
    objdir (binIntRoot .. "/%{prj.name}")

    files
    {
        imguiDir .. "/imconfig.h",
        imguiDir .. "/imgui.h",
        imguiDir .. "/imgui.cpp",
        imguiDir .. "/imgui_draw.cpp",
        imguiDir .. "/imgui_tables.cpp",
        imguiDir .. "/imgui_widgets.cpp",
        imguiDir .. "/imgui_demo.cpp",

        imguiDir .. "/backends/imgui_impl_glfw.h",
        imguiDir .. "/backends/imgui_impl_glfw.cpp",

        imguiDir .. "/backends/imgui_impl_opengl3.h",
        imguiDir .. "/backends/imgui_impl_opengl3.cpp",
        imguiDir .. "/backends/imgui_impl_opengl3_loader.h"
    }

    includedirs
    {
        imguiDir,
        imguiDir .. "/backends",
        glfwDir .. "/include"
    }

    defines
    {
        "GLFW_INCLUDE_NONE"
    }

    filter "system:windows"
        systemversion "latest"
        staticruntime "Off"

        defines
        {
            "IMGUI_API=__declspec(dllexport)"
        }

    filter { "system:windows", "configurations:Debug" }
        runtime "Debug"

    filter { "system:windows", "configurations:Release" }
        runtime "Release"

    filter { "system:windows", "configurations:Dist" }
        runtime "Release"

    filter "system:linux"
        pic "On"

    filter "configurations:Debug"
        symbols "On"

    filter "configurations:Release"
        optimize "On"

    filter "configurations:Dist"
        optimize "On"

    filter {}



--
-- Ensure GLFW uses the same Windows CRT as Hazel and Sandbox.
--
-- staticruntime "Off":
--     Debug   -> /MDd
--     Release -> /MD
--     Dist    -> /MD
--
-- TheCherno's current GLFW Premake script already uses the dynamic
-- runtime, but keeping this explicit makes our intended configuration
-- clear and prevents CRT mismatches.
--

project "GLFW"

    filter "system:windows"
        staticruntime "Off"

    filter { "system:windows", "configurations:Debug" }
        runtime "Debug"

    filter { "system:windows", "configurations:Release" }
        runtime "Release"

    filter { "system:windows", "configurations:Dist" }
        runtime "Release"

    filter {}



-- ================================================================
-- Hazel
-- ================================================================

project "Hazel"

    location "Hazel"

    kind "SharedLib"
    language "C++"
    cppdialect "C++17"

    pchheader "hzpch.h"
    pchsource "Hazel/src/hzpch.cpp"

    targetdir (binRoot .. "/%{prj.name}")
    objdir    (binIntRoot .. "/%{prj.name}")


    files
    {
        "Hazel/src/**.h",
        "Hazel/src/**.cpp"
    }


    includedirs
    {
        "Hazel/src",
        "Hazel/vendor/spdlog/include",
        "Hazel/vendor/GLFW/include",
        "Hazel/vendor/Glad/include",
        "Hazel/vendor/imgui",
        "Hazel/vendor/imgui/backends",
        "Hazel/vendor/glm"
    }


    links
    {
        "GLFW",
        "Glad",
        "ImGui"
    }

    wholearchive { "ImGui" }

    defines
    {
        "GLFW_INCLUDE_NONE"
    }



    -- ============================================================
    -- Windows
    -- ============================================================

    filter "system:windows"

        systemversion "latest"

        --
        -- Use Microsoft's dynamic CRT.
        --
        -- Combined with runtime "Debug"/"Release" below:
        --
        -- Debug   -> /MDd
        -- Release -> /MD
        -- Dist    -> /MD
        --
        staticruntime "Off"

        defines
        {
            "HZ_PLATFORM_WINDOWS",
            "HZ_BUILD_DLL"
        }

        --
        -- Do not compile Linux platform implementation files.
        --
        removefiles
        {
            "Hazel/src/Platform/Linux/**.cpp"
        }

        links
        {
            "opengl32.lib"
        }

        --
        -- Put Hazel.dll directly beside Sandbox.exe.
        --
        -- This avoids needing a fragile post-build copy command.
        --
        targetdir (binRoot .. "/Sandbox")

        --
        -- Keep Hazel.lib in Hazel's own output directory.
        --
        implibdir (binRoot .. "/Hazel")


    --
    -- Windows Debug CRT -> /MDd
    --
    filter { "system:windows", "configurations:Debug" }
        runtime "Debug"


    --
    -- Windows Release CRT -> /MD
    --
    filter { "system:windows", "configurations:Release" }
        runtime "Release"


    --
    -- Windows Distribution CRT -> /MD
    --
    filter { "system:windows", "configurations:Dist" }
        runtime "Release"



    -- ============================================================
    -- Linux
    -- ============================================================

    filter "system:linux"

        toolset "gcc"
        pic "On"

        defines
        {
            "HZ_PLATFORM_LINUX",
            "HZ_BUILD_DLL"
        }

        --
        -- Do not compile Windows platform implementation files.
        --
        removefiles
        {
            "Hazel/src/Platform/Windows/**.cpp"
        }

        links
        {
            "GL",
            "X11",
            "Xrandr",
            "Xi",
            "Xcursor",
            "Xinerama",
            "pthread",
            "dl",
            "m"
        }

        --
        -- Keep libHazel.so in Hazel's output directory.
        --
        targetdir (binRoot .. "/Hazel")



    -- ============================================================
    -- Configurations
    -- ============================================================

    filter "configurations:Debug"

        defines
        {
            "HZ_DEBUG"
        }

        symbols "On"


    filter "configurations:Release"

        defines
        {
            "HZ_RELEASE"
        }

        optimize "On"


    filter "configurations:Dist"

        defines
        {
            "HZ_DIST"
        }

        optimize "On"


    filter {}



-- ================================================================
-- Sandbox
-- ================================================================

project "Sandbox"

    location "Sandbox"

    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"

    targetdir (binRoot .. "/%{prj.name}")
    objdir    (binIntRoot .. "/%{prj.name}")


    files
    {
        "Sandbox/src/**.h",
        "Sandbox/src/**.cpp"
    }


    includedirs
    {
        "Hazel/src",
        "Hazel/vendor/spdlog/include",
        "Hazel/vendor/glm",
        "Hazel/vendor/imgui"
    }


    links
    {
        "Hazel"
    }



    -- ============================================================
    -- Windows
    -- ============================================================

    filter "system:windows"

        systemversion "latest"

        --
        -- Sandbox must use the same CRT as Hazel and GLFW.
        --
        staticruntime "Off"

        defines
        {
            "HZ_PLATFORM_WINDOWS"
        }


    --
    -- Windows Debug CRT -> /MDd
    --
    filter { "system:windows", "configurations:Debug" }
        runtime "Debug"


    --
    -- Windows Release CRT -> /MD
    --
    filter { "system:windows", "configurations:Release" }
        runtime "Release"


    --
    -- Windows Distribution CRT -> /MD
    --
    filter { "system:windows", "configurations:Dist" }
        runtime "Release"



    -- ============================================================
    -- Linux
    -- ============================================================

    filter "system:linux"

        toolset "gcc"

        defines
        {
            "HZ_PLATFORM_LINUX"
        }

        --
        -- libHazel.so stays in:
        --
        -- bin/<config>-linux-x86_64/Hazel/
        --
        -- Tell Linux's dynamic loader to search that directory
        -- when launching Sandbox.
        --
        runpathdirs
        {
            binRoot .. "/Hazel"
        }



    -- ============================================================
    -- Configurations
    -- ============================================================

    filter "configurations:Debug"

        defines
        {
            "HZ_DEBUG"
        }

        symbols "On"


    filter "configurations:Release"

        defines
        {
            "HZ_RELEASE"
        }

        optimize "On"


    filter "configurations:Dist"

        defines
        {
            "HZ_DIST"
        }

        optimize "On"


    filter {}