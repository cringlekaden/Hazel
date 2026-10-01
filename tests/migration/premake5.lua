local repoRoot = _MAIN_SCRIPT_DIR

project "MigrationRendererSmoke"
    location (repoRoot .. "/build/MigrationRendererSmoke")
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    warnings "Extra"
    staticruntime "Off"
    targetdir (repoRoot .. "/bin/" .. outputdir .. "/%{prj.name}")
    objdir (repoRoot .. "/bin-int/" .. outputdir .. "/%{prj.name}")
    files { repoRoot .. "/tests/migration/RendererSmoke.cpp" }
    includedirs { repoRoot .. "/Hazel/src" }
    externalincludedirs
    {
        repoRoot .. "/Hazel/vendor/spdlog/include",
        repoRoot .. "/Hazel/vendor/glm",
        repoRoot .. "/Hazel/vendor/Glad/include",
        repoRoot .. "/Hazel/vendor/GLFW/include"
    }
    defines { "GLFW_INCLUDE_NONE" }
    links { "Hazel", "ImGui", "GLFW", "Glad" }

    filter "system:linux"
        defines { "HZ_PLATFORM_LINUX" }
        toolset "gcc"
        links { "GL", "X11", "Xrandr", "Xi", "Xcursor", "Xinerama", "pthread", "dl", "m" }
    filter "system:windows"
        defines { "HZ_PLATFORM_WINDOWS" }
        systemversion "latest"
        links { "opengl32" }
    filter { "system:windows", "action:vs*" }
        buildoptions { "/utf-8" }
    filter "configurations:Debug"
        defines { "HZ_DEBUG" }
        runtime "Debug"
        symbols "On"
    filter "configurations:Release"
        defines { "HZ_RELEASE" }
        runtime "Release"
        optimize "On"
    filter "configurations:Dist"
        defines { "HZ_DIST" }
        runtime "Release"
        optimize "On"
    filter {}
