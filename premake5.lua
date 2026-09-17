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



-- ================================================================
-- Dependencies
-- ================================================================

group "Dependencies"
    include "Hazel/vendor/GLFW"
    include "Hazel/vendor/Glad"
group ""


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
        "Hazel/vendor/Glad/include"
    }


    links
    {
        "GLFW",
        "Glad"
    }

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
        "Hazel/vendor/spdlog/include"
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