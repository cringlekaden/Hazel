if os.target() == "linux" then _OPTIONS["dotnet"] = "mono" end
local output = assert(os.getenv("HAZEL_GAME_TEST_OUTPUT"))
workspace "ExampleGameTests"
    location (output .. "/Projects")
    architecture "x64"
    configurations { "Debug", "Release" }
project "ExampleGameTests"
    location (output .. "/Projects")
    kind "ConsoleApp"
    language "C#"
    dotnetframework "4.7.2"
    targetdir (assert(os.getenv("HAZEL_GAME_TEST_OUTPUT")))
    objdir (os.getenv("HAZEL_GAME_TEST_OUTPUT") .. "/Intermediates")
    files { path.getabsolute("LightkeeperTests.cs"), path.getabsolute("../../examples/LastLightkeeper/Assets/Scripts/Source/Rules.cs"), path.getabsolute("FlightTests.cs"), path.getabsolute("../../examples/Skybound/Assets/Scripts/Source/Flight.cs") }
    links { "System" }
    filter "system:linux"
        buildoptions { "-sdk:4.7.2" }
    filter "configurations:Debug"
        symbols "Default"
    filter "configurations:Release"
        optimize "On"
    filter {}
