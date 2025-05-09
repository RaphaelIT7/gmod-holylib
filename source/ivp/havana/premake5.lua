function IncludeHavanaConstraints()
	links("havana_constraints")
	project("havana_constraints")
		kind("StaticLib")
		language("C++")
		targetdir("%{wks.location}/bin/%{cfg.buildcfg}")
		objdir("%{wks.location}/obj/%{cfg.buildcfg}")

		-- pchheader("havana_constraints.pch")

		includedirs({
			"../ivp_intern",
			"../ivp_collision",
			"../ivp_physics",
			"../ivp_surface_manager",
			"../ivp_utility",
			"../ivp_controller",
			"../ivp_compact_builder",
			"havok",
		})

		defines({
			"VPHYSICS_EXPORTS",
			"HAVANA_CONSTRAINTS",
			"HAVOK_MOPP",
			"IVP_VERSION_SDK"
		})

		files({
			"havok/hk_physics/core/vm_query_builder/*.h",

			"havok/hk_physics/constraint/*.cpp",
			"havok/hk_physics/constraint/*.h",

			"havok/hk_physics/constraint/ball_socket/*.cpp",
			"havok/hk_physics/constraint/ball_socket/*.h",

			"havok/hk_physics/constraint/limited_ball_socket/*.cpp",
			"havok/hk_physics/constraint/limited_ball_socket/*.h",

			"havok/hk_physics/constraint/ragdoll/*.cpp",
			"havok/hk_physics/constraint/ragdoll/*.h",

			"havok/hk_physics/constraint/local_constraint_system/*.cpp",
			"havok/hk_physics/constraint/local_constraint_system/*.h",

			"havok/hk_physics/constraint/util/*.h",

			"havok/hk_physics/constraint/hinge/*.cpp",
			"havok/hk_physics/constraint/hinge/*.h",

			"havok/hk_physics/constraint/breakable_constraint/*.cpp",
			"havok/hk_physics/constraint/breakable_constraint/*.h",

			"havok/hk_physics/constraint/fixed/*.cpp",
			"havok/hk_physics/constraint/fixed/*.h",

			"havok/hk_physics/constraint/prismatic/*.cpp",
			"havok/hk_physics/constraint/prismatic/*.h",

			"havok/hk_physics/constraint/pulley/*.cpp",
			"havok/hk_physics/constraint/pulley/*.h",

			"havok/hk_physics/constraint/stiff_spring/*.cpp",
			"havok/hk_physics/constraint/stiff_spring/*.h",

			"havok/hk_physics/physics.h",
			"havok/hk_physics/simunit/psi_info.h",
			"havok/hk_physics/core/rigid_body_core.cpp",
			"havok/hk_physics/effector/rigid_body_binary_effector.cpp"
		})

		vpaths({
			["hk_physics (havana)/core/vm_query_builder"] = "**/vm_query_builder",
			["hk_physics (havana)/constraint"] = "**constraint/constraint*.*",
			["hk_physics (havana)/ball_socket"] = "**constraint/ball_socket/**",
			["hk_physics (havana)/limited_ball_socket"] = "**constraint/limited_ball_socket/**",
			["hk_physics (havana)/ragdoll"] = "**constraint/ragdoll/**",
			["hk_physics (havana)/local_constraint_system"] = "**constraint/local_constraint_system/**",
			["hk_physics (havana)/util"] = "**constraint/util/**",
			["hk_physics (havana)/hinge"] = "**constraint/hinge/**",
			["hk_physics (havana)/breakable_constraint"] = "**constraint/breakable_constraint/**",
			["hk_physics (havana)/fixed"] = "**constraint/fixed/**",
			["hk_physics (havana)/prismatic"] = "**constraint/prismatic/**",
			["hk_physics (havana)/pulley"] = "**constraint/pulley/**",
			["hk_physics (havana)/stiff_spring"] = "**constraint/stiff_spring/**",

			["hk_physics (ipion)"] = "**hk_physics/*.*",
			["hk_physics (ipion)/simunit"] = "**hk_physics/simunit/*.*",
			["hk_physics (ipion)/core"] = "**hk_physics/core/*",
			["hk_physics (ipion)/effector"] = "**hk_physics/effector/*",
			
			["Other"] = "std*",
		})

		filter("configurations:Debug")
			symbols "On"
			runtime "Debug"
			defines { "DEBUG" }
			buildoptions { "RTC1" }

		filter("configurations:Release")
			optimize("Speed")
			runtime("Release")
			defines({"NDEBUG"})
			buildoptions({"Ob1"})

		filter("system:windows")
			defines({"_WIN32", "WIN32"})

		filter("system:linux")
			defines({"_LINUX", "LINUX", "POSIX"})

		filter({})
end
IncludeHavanaConstraints()