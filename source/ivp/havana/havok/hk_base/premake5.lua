function IncludeHKBase()
	links("hk_base")

	project("hk_base")
		kind("StaticLib")
		language("C++")
		cppdialect("C++17")
		staticruntime("on")
		characterset("MBCS")

		targetdir("%{wks.location}/bin/%{cfg.buildcfg}")
		objdir("%{wks.location}/obj/%{cfg.buildcfg}")

		-- pchheader("base.h")
		-- pchsource("base_types.cpp")

		includedirs({
			"../",
			"../../havana"
		})

		defines({
			"VPHYSICS_EXPORTS",
			"HAVANA_CONSTRAINTS",
			"HAVOK_MOPP"
		})

		files({
			"memory/**.cpp",
			"memory/**.h",
			"memory/**.inl",
			"array/**.cpp",
			"array/**.h",
			"array/**.inl",
			"hash/**.cpp",
			"hash/**.h",
			"hash/**.inl",
			"stopwatch/**.cpp",
			"stopwatch/**.h",
			"string/**.cpp",
			"string/**.h",
			"id_server/**.cpp",
			"id_server/**.h",
			"id_server/**.inl",
			"stdafx.h",
			"base.h",
			"base_types.cpp",
			"base_types.h",
			"console.cpp",
			"console.h"
		})

		filter("configurations:Debug")
			symbols("On")
			runtime("Debug")
			defines({"DEBUG"})
			buildoptions({"/RTC1"})
			pchheader("base.h")
			pchsource("base_types.cpp")

		filter("configurations:Release")
			optimize("Speed")
			runtime("Release")
			defines({"NDEBUG"})
			buildoptions({"/Ob1"})
			pchheader("base.h")
			pchsource("base_types.cpp")

		filter("system:windows")
			defines({"_WIN32", "WIN32"})

		filter("system:linux")
			defines({"_LINUX", "LINUX", "POSIX"})

		filter({})
end
IncludeHKBase()