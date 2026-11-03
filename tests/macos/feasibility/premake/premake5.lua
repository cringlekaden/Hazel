-- Maintained stage 0-1 probe for the project-owned Premake binary. This is
-- intentionally independent of the not-yet-ported Hazel application targets.
local output = os.getenv("HAZEL_PREMAKE_PROBE_OUTPUT") or
    path.getabsolute("../../../../build/testing/macos/premake", _MAIN_SCRIPT_DIR)

workspace "HazelMacOSPremakeProbe"
    location (output)
    architecture "ARM64"
    configurations { "Release" }
    systemversion "13.0"

project "PremakeProbe"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    targetdir (output .. "/bin")
    objdir (output .. "/obj")
    files { "main.cpp", "bridge.mm" }
    links { "Foundation.framework" }
    filter "system:macosx"
        toolset "clang"
        -- The pinned gmake backend applies systemversion to compilation but
        -- omits it from the final link command. Keep the target explicit until
        -- the pin is changed or the upstream generator is fixed.
        linkoptions { "-mmacosx-version-min=13.0" }
    filter "configurations:Release"
        optimize "On"
    filter {}
