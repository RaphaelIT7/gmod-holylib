function IncludeIVPCompactBuilder()
	links("ivp_compactbuilder")
	project("ivp_compactbuilder")
		kind("StaticLib")
		language("C++")
		cppdialect("C++17")
		staticruntime("on")
		characterset("MBCS")

		targetdir("%{wks.location}/bin/%{cfg.buildcfg}")
		objdir("%{wks.location}/obj/%{cfg.buildcfg}")

		pchheader("ivp_physics.hxx")
		pchsource("../ivp_compact_builder/ivp_compact_ledge_gen.cxx")

		includedirs({
			"../ivp_intern",
			"../ivp_collision",
			"../ivp_physics",
			"../ivp_surface_manager",
			"../ivp_utility",
			"../ivp_controller",
			"../ivp_compact_builder",
			"../havana/havok",
			"../havana"
		})

		files({
			"../ivp_compact_builder/**.cxx",
			"../ivp_compact_builder/**.hxx",
			"../ivp_compact_builder/**.h"
		})

		removefiles({
			"../ivp_compact_builder/3d**.cxx"
		})

		vpaths({
			["IVP_QHULL"] = "qhull*",
			["IVP_GEOMPACK"] = "geompack*",
			["IVP_COMPACT_BUILDER"] = "iv*",
			["Source Files"] = "3ds*",
			["Other"] = "std*",
		})

		defines({
			"IVP_VERSION_SDK",
			"HAVOK_MOPP"
		})

		filter("configurations:Debug")
			defines({"_DEBUG"})
			symbols("On")
			runtime("Debug")
			filter("system:windows")
				buildoptions({"/RTC1"})

		filter("configurations:Release")
			defines({"NDEBUG"})
			optimize("Speed")
			runtime("Release")
			filter("system:windows")
				buildoptions({"/Ob1"})

		filter("system:windows")
			defines({"_WIN32", "WIN32"})

		filter("system:linux")
			defines({"_LINUX", "LINUX", "POSIX"})

		filter({})
end
IncludeIVPCompactBuilder()