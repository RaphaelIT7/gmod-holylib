function IncludeIVPPhysics()
	project("ivp_physics")
		kind("StaticLib")
		language("C++")
		cppdialect("C++17")
		staticruntime("on")
		characterset("MBCS")

		targetdir("%{wks.location}/bin/%{cfg.buildcfg}")
		objdir("%{wks.location}/bin-int/%{cfg.buildcfg}")

		pchheader("ivp_physics.hxx")
		pchsource("../ivp_intern/ivp_physic.cxx")

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
			"../ivp_collision/**.cxx",
			"../ivp_collision/**.hxx",

			"../ivp_controller/**.cxx",
			"../ivp_controller/**.hxx",
			"../ivp_controller/**.cpp",
			"../ivp_controller/**.h",

			"../ivp_intern/**.cxx",
			"../ivp_intern/**.hxx",

			"../ivp_physics/**.cxx",
			"../ivp_physics/**.hxx",

			"../ivp_surface_manager/**.cxx",
			"../ivp_surface_manager/**.hxx",

			"../ivp_utility/**.cxx",
			"../ivp_utility/**.hxx"
		})

		vpaths({
			["IVP_COLLISION"] = "**/ivp_collision/**",
			["IVP_CONTROLLER"] = "**/ivp_controller/**",
			["IVP_INTERN"] = "**/ivp_intern/**",
			["IVP_PHYSICS"] = "**/ivp_physics/**",
			["IVP_SURFACEMANAGER"] = "**/ivp_surface_manager/**",
			["IVP_UTILITY"] = "**/ivp_utility/**",
			
			["Other"] = "std*",
		})

		defines({
			"VPHYSICS_EXPORTS",
			"IVP_VERSION_SDK",
			"HAVANA_CONSTRAINTS",
			"HAVOK_MOPP"
		})

		filter("configurations:Debug")
			defines({"DEBUG"})
			symbols("On")
			runtime("Debug")
			optimize("Off")

		filter("configurations:Release")
			symbols("Off")
			optimize("Speed")
			runtime("Release")

		filter("system:windows")
			defines({"_WIN32", "WIN32"})

		filter("system:linux")
			defines({"_LINUX", "LINUX", "POSIX"})

		filter({})
end
IncludeIVPPhysics()