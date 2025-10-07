--[[
	Dependency manager used for HolyLib files to exclude them from builds based of their dependencies.

	Current working keywords to use inside cpp files:
	
	HOLYLIB_REQUIRES_MODULE=moduleName1,moduleName2
	Usage: if the given module cannot be found inside the source/modules/ folder using moduleName.cpp then the file the comment is defined in is excluded from being compiled.

	
	NOTE: This file is directly loaded by premake5! So we need to use normal lua syntax!
]]

dofile("utils.lua")

local _ProcessDir -- for recursion :3
local function ProcessDir(results, dirPath, files)
	-- print("ProcessDir called : ", dirPath, files)
	for _dirname, filename in pairs(files) do
		if type(filename) == "table" then
			local fullPath = dirPath .. (string.EndsWith(dirPath, "/") and "" or "/") .. _dirname
			_ProcessDir(results, fullPath, filename)
		elseif EndsWith(filename, ".cpp") then
			local fullPath = dirPath .. (string.EndsWith(dirPath, "/") and "" or "/") .. filename
			local content = ReadFile(fullPath)
			if content then
				local requires = content:match("HOLYLIB_REQUIRES_MODULE=([^\r\n]+)")
				if requires then
					for mod in requires:gmatch("([^,]+)") do
						mod = string.Trim(mod)
						local modulePath = "source/modules/" .. mod .. ".cpp"
						if not ReadFile(modulePath) then
							table.insert(results, fullPath)
							break
						end
					end
				end
			end
		end
	end
end
_ProcessDir = ProcessDir

function GetExcludedFiles(basePath, makeRelative)
	local excluded = {}
	local sourceFiles = ScanDir(basePath, true)

	ProcessDir(excluded, basePath, sourceFiles)

	if makeRelative then
		-- Now we make the results relative to the source/ directory
		for key, path in ipairs(excluded) do
			excluded[key] = path:sub(basePath:len() + 1)
		end
	end

	return excluded
end