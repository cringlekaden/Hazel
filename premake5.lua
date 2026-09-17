--
-- Hazel
-- Cross-platform Premake configuration
--
-- Supported:
--   Windows x86_64 - Visual Studio / MSVC
--   Linux   x86_64 - GNU Make / GCC
--
-- Projects:
--   Hazel   - shared engine library
--   Sandbox - executable using Hazel
--

local workspaceRoot = path.getabsolute(".")

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
-- Output directory naming
--
-- Examples:
--
-- Debug-windows-x86_64
-- Debug-linux-x86_64
--
outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"


--
-- Common absolute output locations.
--
-- Using absolute paths here prevents generated project files located
-- inside Hazel/ or Sandbox/ from accidentally rebasing paths into
-- ../../bin or other incorrect locations.
--

local binRoot    = workspaceRoot .. "/bin/" .. outputdir
local binIntRoot = workspaceRoot .. "/bin-int/" .. outputdir

group "Dependencies"
    include "Hazel/vendor/GLFW"
group ""

project "GLFW"
    filter "system:windows"
        staticruntime "On"

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
        "Hazel/vendor/GLFW/include"
    }

    links
    {
        "GLFW"
    }


    --
    -- Windows
    --
    -- HZ_BUILD_DLL tells Core.h that Hazel is PRODUCING
    -- the DLL, so HAZEL_API becomes __declspec(dllexport).
    --
    filter "system:windows"

        systemversion "latest"
        staticruntime "On"

        defines
        {
            "HZ_PLATFORM_WINDOWS",
            "HZ_BUILD_DLL"
        }

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
        -- This achieves the same runtime layout as Cherno's
        -- post-build DLL copy without relying on fragile shell
        -- copy commands.
        --
        targetdir (binRoot .. "/Sandbox")

        --
        -- Keep Hazel.lib in Hazel's own output directory.
        --
        -- Sandbox links against this import library.
        --
        implibdir (binRoot .. "/Hazel")


    --
    -- Linux
    --
    -- HAZEL_API is intentionally blank on Linux for now.
    -- Symbols in the shared object are visible normally.
    --
    filter "system:linux"

        toolset "gcc"
        pic "On"

        defines
        {
            "HZ_PLATFORM_LINUX",
            "HZ_BUILD_DLL"
        }

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
        -- Linux keeps libHazel.so in Hazel's output directory.
        --
        targetdir (binRoot .. "/Hazel")


    --
    -- Debug
    --
    filter "configurations:Debug"

        defines
        {
            "HZ_DEBUG"
        }

        symbols "On"


    --
    -- Release
    --
    filter "configurations:Release"

        defines
        {
            "HZ_RELEASE"
        }

        optimize "On"


    --
    -- Distribution
    --
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
        "Hazel/vendor/spdlog/include"
    }

    links
    {
        "Hazel"
    }


    --
    -- Windows
    --
    -- Sandbox CONSUMES Hazel.dll.
    --
    -- HZ_BUILD_DLL is deliberately NOT defined here, therefore
    -- HAZEL_API becomes __declspec(dllimport).
    --
    filter "system:windows"

        systemversion "latest"
        staticruntime "On"

        defines
        {
            "HZ_PLATFORM_WINDOWS"
        }


    --
    -- Linux
    --
    filter "system:linux"

        toolset "gcc"

        defines
        {
            "HZ_PLATFORM_LINUX"
        }

        --
        -- Tell the Linux dynamic loader where libHazel.so lives.
        --
        -- Premake turns this into the appropriate runtime search
        -- path during linking. No manual -rpath/-Xlinker flags and
        -- no copying of libHazel.so are required.
        --
        runpathdirs
        {
            binRoot .. "/Hazel"
        }


    --
    -- Debug
    --
    filter "configurations:Debug"

        defines
        {
            "HZ_DEBUG"
        }

        symbols "On"


    --
    -- Release
    --
    filter "configurations:Release"

        defines
        {
            "HZ_RELEASE"
        }

        optimize "On"


    --
    -- Distribution
    --
    filter "configurations:Dist"

        defines
        {
            "HZ_DIST"
        }

        optimize "On"


    filter {}