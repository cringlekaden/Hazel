-- Root-owned integration for clean, separately built shader libraries.
local repoRoot = _MAIN_SCRIPT_DIR
ShaderToolsRoot = repoRoot .. "/build/dependencies/install/" .. outputdir
ShaderToolsIncludes = ShaderToolsRoot .. "/include"
ShaderToolsLinks = { "shaderc_combined", "spirv-cross-glsl", "spirv-cross-core" }

project "MigrationShaderToolsSmoke"
    location (repoRoot .. "/build/MigrationShaderToolsSmoke")
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    staticruntime "Off"
    warnings "Extra"
    targetdir (repoRoot .. "/bin/" .. outputdir .. "/%{prj.name}")
    objdir (repoRoot .. "/bin-int/" .. outputdir .. "/%{prj.name}")
    files { repoRoot .. "/tests/migration/ShaderToolsSmoke.cpp" }
    externalincludedirs { ShaderToolsIncludes }
    libdirs { ShaderToolsRoot .. "/lib" }
    links (ShaderToolsLinks)
    filter "system:linux"
        toolset "gcc"
        links { "pthread", "dl", "m" }
    filter "system:windows"
        systemversion "latest"
    filter { "system:windows", "action:vs*" }
        buildoptions { "/utf-8" }
    filter "configurations:Debug"
        runtime "Debug"
        symbols "On"
    filter "configurations:Release"
        runtime "Release"
        optimize "On"
    filter "configurations:Dist"
        runtime "Release"
        optimize "On"
        -- The Dist consumer uses the Release toolchain and CRT.
        externalincludedirs { repoRoot .. "/build/dependencies/install/Release-%{cfg.system}-%{cfg.architecture}/include" }
        libdirs { repoRoot .. "/build/dependencies/install/Release-%{cfg.system}-%{cfg.architecture}/lib" }
    filter {}
