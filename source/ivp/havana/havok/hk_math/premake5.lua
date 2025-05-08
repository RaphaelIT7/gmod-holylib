function IncludeHKMath()
	project("hk_math")
		kind("StaticLib")
		language("C++")
		cppdialect("C++17")
		staticruntime("on")
		characterset("MBCS")

		targetdir("%{wks.location}/bin/%{cfg.buildcfg}")
		objdir("%{wks.location}/obj/%{cfg.buildcfg}")

		-- pchheader("vecmath.h")
		-- pchsource("densematrix.cpp")

		includedirs({
			"../",
			"../../",
			"../../../ivp_utility"
		})

		files({
			"lcp/**.cpp",
			"lcp/**.h",

			"incr_lu/**.cpp",
			"incr_lu/**.h",

			"gauss_elimination/**.cpp",
			"gauss_elimination/**.h",

			"quaternion/**.cpp",
			"quaternion/**.h",
			"quaternion/**.inl",

			"vector3/**.cpp",
			"vector3/**.h",
			"vector3/**.inl",

			"matrix/**.h",

			"vector_fpu/**.h",
			"vector_fpu/**.inl",

			-- "*.h",
			-- "*.inl",
			-- "*.cpp",
			"densematrix.h",
			"densematrix.inl",
			"densematrix_util.cpp",
			"densematrix_util.h",
			"densematrix_util.inl",
			"eulerangles.cpp",
			"eulerangles.h",
			"interval.h",
			"math.cpp",
			"base_math.h",
			"math.inl",
			"matrix3.cpp",
			"matrix3.h",
			"matrix3.inl",
			"odesolve.cpp",
			"odesolve.h",
			"plane.cpp",
			"plane.h",
			"plane.inl",
			"qtransform.h",
			"qtransform.inl",
			"ray.h",
			"ray.inl",
			"rotation.cpp",
			"rotation.h",
			"rotation.inl",
			"spatial_matrix.cpp",
			"spatial_matrix.h",
			"spatial_matrix.inl",
			"spatial_vector.cpp",
			"spatial_vector.h",
			"spatial_vector.inl",
			"transform.cpp",
			"transform.h",
			"transform.inl",
			"types.h",
			"vecmath.h",
			"vector4.h",
			"vector4.inl",
			"stdafx.h"
		})

		vpaths({
			["lcp"] = "lcp/*",
			["incr_lu"] = "incr_lu/*",
			["gauss_elimination"] = "gauss_elimination/*",
			["quaternion"] = "quaternion/*",
			["vector3"] = "vector3/*",
			["matrix"] = "matrix/*",
			["vector_fpu"] = "vector_fpu/*",

			["source"] = "*.*"
		})

		filter("configurations:Debug")
			symbols("On")
			runtime("Debug")
			defines({"_DEBUG", "VPHYSICS_EXPORTS", "HAVANA_CONSTRAINTS", "HAVOK_MOPP", "_LIB", "HK_DEBUG"})
			buildoptions({"/RTC1", "/Od"})
			staticruntime("On")

		filter("configurations:Release")
			optimize("Speed")
			runtime("Release")
			defines({"VPHYSICS_EXPORTS", "HAVANA_CONSTRAINTS", "HAVOK_MOPP"})
			buildoptions({"/Ob1"})

		filter("system:windows")
			defines({"_WIN32", "WIN32"})

		filter("system:linux")
			defines({"_LINUX", "LINUX", "POSIX"})

		filter({})
end
IncludeHKMath()