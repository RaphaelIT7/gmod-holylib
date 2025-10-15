local current_dir = _SCRIPT_DIR
function runFile(fileName)
	local _project = project()
	group("IVP")
	dofile(current_dir .. "/" .. fileName)
	defines({
		"IVP_NO_PERFORMANCE_TIMER",
		"IVP_NO_DEBUGMANAGER"
	})
	filter({"platforms:x86_64"})
		defines("PLATFORM_64BITS")

	project(_project.name)
end

function IncludeIVP()
	defines({"CUSTOM_VPHYSICS_BUILD"})

	IncludePackage("ivp")
	runFile("havana/premake5.lua")
	runFile("havana/havok/hk_base/premake5.lua")
	runFile("havana/havok/hk_math/premake5.lua")
	runFile("ivp_physics/premake5.lua")
	runFile("ivp_compact_builder/premake5.lua")

	dofile(current_dir .. "/vphysics/premake5.lua") -- Unlike the others, vphysics WONT be a seperate project
end