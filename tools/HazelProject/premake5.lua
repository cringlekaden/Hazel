local root = _MAIN_SCRIPT_DIR
project "HazelProject"
    location (root .. "/build/HazelProject")
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    staticruntime "Off"
    targetdir (root .. "/bin/" .. outputdir .. "/%{prj.name}")
    objdir (root .. "/bin-int/" .. outputdir .. "/%{prj.name}")
    files { root .. "/tools/HazelProject/main.cpp" }
    includedirs { root .. "/Hazel/src" }
    externalincludedirs {root .. "/Hazel/vendor/spdlog/include",root .. "/Hazel/vendor/glm",root .. "/Hazel/vendor/imgui"}
    links {"Hazel", "ImGui", "GLFW", "Glad"}
    filter "system:linux"
        defines {"HZ_PLATFORM_LINUX"}
        links (MigrationLinuxLinks)
        links {"GL", "X11", "Xrandr", "Xi", "Xcursor", "Xinerama", "pthread", "dl", "m"}
    filter "system:windows"
        defines {"HZ_PLATFORM_WINDOWS"}
        systemversion "latest"
        links {"opengl32", "Comdlg32", "Shell32", "Ole32", "Advapi32"}
    filter {"system:windows", "action:vs*"}
        buildoptions {"/utf-8"}
    filter "configurations:Debug"
        runtime "Debug"
        defines {"HZ_DEBUG"}
        symbols "On"
    filter "configurations:Release"
        runtime "Release"
        defines {"HZ_RELEASE"}
        optimize "On"
    filter "configurations:Dist"
        runtime "Release"
        defines {"HZ_DIST"}
        optimize "On"
    filter {}
