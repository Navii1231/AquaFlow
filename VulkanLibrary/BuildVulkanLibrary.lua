outputDir = "%{cfg.buildcfg}/%{cfg.architecture}"

project "VulkanLibrary"
	location ""
	kind "StaticLib"
	language "C++"

	targetdir ("../out/bin/" .. outputDir .. "/%{prj.name}")
    objdir ("../out/int/" .. outputDir .. "/%{prj.name}")
    flags {"MultiProcessorCompile"}

    pchheader "Core/vkpch.h"
    pchsource "%{prj.location}/src/Core/vkpch.cpp"

	files
	{
		"%{prj.location}/**.h",
		"%{prj.location}/**.hpp",
		"%{prj.location}/**.c",
		"%{prj.location}/**.cpp",
        "%{prj.location}/**.txt",
        "%{prj.location}/**.lua",
	}

	includedirs
	{
		"%{prj.location}/Dependencies/include/",
		"%{prj.location}/Include/",
	}

    libdirs
    {
    	"%{prj.location}/Dependencies/lib/",
    }

    links
    {
        "glfw3.lib",
        "vulkan-1.lib"
    }

		filter "system:windows"
        cppdialect "C++20"
        staticruntime "On"
        systemversion "10.0"

        defines
        {
            "_CONSOLE",
            "WIN32",
        }

        filter "configurations:Debug"
            defines
            {
                "VULKAN_ENGINE_BUILD_STATIC",
                "_DEBUG",
            }

            inlining "Disabled"
            symbols "On"
            staticruntime "Off"
            runtime "Debug"

        filter "configurations:Release"

            defines "NDEBUG"
            optimize "Full"
            inlining "Auto"
            staticruntime "Off"
            runtime "Release"
