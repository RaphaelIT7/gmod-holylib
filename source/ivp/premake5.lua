local current_dir = _SCRIPT_DIR
function IncludeIVP()
	local _project = project()
	dofile(current_dir .. "/havana/premake5.lua")
	dofile(current_dir .. "/havana/havok/hk_base/premake5.lua")
	dofile(current_dir .. "/havana/havok/hk_math/premake5.lua")
	dofile(current_dir .. "/ivp_physics/premake5.lua")
	dofile(current_dir .. "/ivp_compact_builder/premake5.lua")
	project(_project.name)
end