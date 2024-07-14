local current_dir = _SCRIPT_DIR

function IncludeBootil()
	IncludePackage("bootil")

	local _project = project()

	externalincludedirs(current_dir .. "/src/3rdParty")
	externalincludedirs(current_dir .. "/include")

	filter({})
end