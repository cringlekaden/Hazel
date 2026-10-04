-- Root-owned integration for clean, separately built shader libraries.
local repoRoot = _MAIN_SCRIPT_DIR
ShaderToolsRoot = repoRoot .. "/build/dependencies/install/" .. outputdir
ShaderToolsIncludes = ShaderToolsRoot .. "/include"
ShaderToolsLinks = { "shaderc_combined", "spirv-cross-glsl", "spirv-cross-core" }

if _OPTIONS["shader-tools"] then
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

end

-- The engine needs the public headers; final native applications resolve its
-- static shader-library references using matching configuration/CRT libraries.
project "Hazel"
    externalincludedirs { ShaderToolsIncludes }
    filter "configurations:Dist"
        externalincludedirs { repoRoot .. "/build/dependencies/install/Release-%{cfg.system}-%{cfg.architecture}/include" }
    filter {}

local consumers = { "Nutella", "Hazelnut", "SpriteAssetAudit" }
if _OPTIONS["migration-tests"] then
    table.insert(consumers, "MigrationRendererSmoke")
    table.insert(consumers, "MigrationCoreSmoke")
    table.insert(consumers, "MigrationRendererFeaturesSmoke")
    table.insert(consumers, "MigrationSceneFoundationSmoke")
    table.insert(consumers, "MigrationFontSmoke")
    table.insert(consumers, "MigrationRenderer2DSmoke")
    table.insert(consumers, "MigrationProjectPhysicsSmoke")
    table.insert(consumers, "MigrationMonoSmoke")
    table.insert(consumers, "MigrationSceneSmoke")
    table.insert(consumers, "MigrationSceneGPUSmoke")
    table.insert(consumers, "MigrationEditorSmoke")
    table.insert(consumers, "MigrationRuntimeSessionSmoke")
    table.insert(consumers, "MigrationExampleGamesSmoke")
    table.insert(consumers, "MigrationSpriteSmoke")
end
for _, consumer in ipairs(consumers) do
    project (consumer)
        libdirs { ShaderToolsRoot .. "/lib" }
        links (ShaderToolsLinks)
        filter "configurations:Dist"
            libdirs { repoRoot .. "/build/dependencies/install/Release-%{cfg.system}-%{cfg.architecture}/lib" }
        filter {}
end
