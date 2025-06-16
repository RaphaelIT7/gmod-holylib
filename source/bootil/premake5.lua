local current_dir = _SCRIPT_DIR
function IncludeBootil()
	IncludePackage("bootil")

	local _project = project()
	group("bootil")

	includedirs({
		current_dir .. "/include/",
	})

	print(current_dir)
	links("bootil")
	project("bootil")
		kind("StaticLib")
		language("C++")
		cppdialect("C++17")
		staticruntime("on")
		characterset("MBCS")

		targetdir("%{wks.location}/bin/%{cfg.buildcfg}")
		objdir("%{wks.location}/bin-int/%{cfg.buildcfg}")

		includedirs({
			current_dir .. "/include/",
			current_dir .. "/src/3rdParty",
		})

		files({
			current_dir .. "/**",
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
	project(_project.name)
end