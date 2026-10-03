-- Actual Hazelnut project with portable paths and matching dynamic CRT.
local root = _MAIN_SCRIPT_DIR
project "Hazelnut"
    location (root .. "/build/Hazelnut")
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    staticruntime "Off"
    targetdir (root .. "/bin/" .. outputdir .. "/%{prj.name}")
    objdir (root .. "/bin-int/" .. outputdir .. "/%{prj.name}")
    debugdir (root)
    files { "src/**.h", "src/**.cpp" }
    includedirs { root .. "/Hazel/src", root .. "/Hazelnut/src" }
    externalincludedirs { root .. "/Hazel/vendor/spdlog/include", root .. "/Hazel/vendor/imgui", root .. "/Hazel/vendor/glm", root .. "/Hazel/vendor/ImGuizmo" }
    links { "Hazel", "ImGui", "GLFW", "Glad", "ImGuizmo" }
    dependson { "Hazel-ScriptCore", "ExampleScripts" }
    filter "system:linux"
        defines { "HZ_PLATFORM_LINUX" }
        externalincludedirs (HazelGTKIncludes)
        links (MigrationLinuxLinks)
        links { "GL", "X11", "Xrandr", "Xi", "Xcursor", "Xinerama", "pthread", "dl", "m" }
    filter "system:windows"
        defines { "HZ_PLATFORM_WINDOWS" }
        systemversion "latest"
        links { "opengl32", "Comdlg32", "Shell32" }
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
