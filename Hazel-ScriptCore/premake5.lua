-- Actual target managed API/project, isolated per-configuration outputs.
local root = _MAIN_SCRIPT_DIR
project "Hazel-ScriptCore"
    location (root .. "/build/Hazel-ScriptCore")
	kind "SharedLib"
	language "C#"
	dotnetframework "4.7.2"
    links { "System", "System.Core" }

	targetdir (root .. "/bin/" .. outputdir .. "/%{prj.name}")
	objdir (root .. "/bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"Source/**.cs",
		"Properties/**.cs"
	}

	filter "configurations:Debug"
		optimize "Off"
		symbols "Default"

	filter "configurations:Release"
		optimize "On"
		symbols "Default"

	filter "configurations:Dist"
		optimize "Full"
		symbols "Off"

    filter "system:linux"
        buildoptions { "-sdk:4.7.2" }
    filter {}
