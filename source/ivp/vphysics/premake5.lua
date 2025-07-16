function IncludeVPhysics()
	includedirs({
		"../vphysics/",
		"../ivp_intern",
		"../ivp_collision",
		"../ivp_physics",
		"../ivp_surface_manager",
		"../ivp_utility",
		"../ivp_controller",
		"../ivp_compact_builder",
		"../havana/havok",
		"../havana",
	})

	defines("VPHYSICS_EXPORTS")
	defines("HAVANA_CONSTRAINTS")
	defines("HAVOK_MOPP")

	files({
		"*.cpp",
		"*.h",
	})
end
IncludeVPhysics()