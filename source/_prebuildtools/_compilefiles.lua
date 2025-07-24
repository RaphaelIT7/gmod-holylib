dofile("utils.lua")

local function CompileLuaScipts()
	local path = "../lua/scripts/"
	for _, fileName in ipairs(ScanDir(path, false)) do
		if not EndsWith(fileName, ".lua") then continue end

		local headerFileName = path .. RemoveEnd(fileName, ".lua") .. ".h"
		local headerFile = [[const char* lua]] .. RemoveEnd(fileName, ".lua") .. [[ = R"LUAFILE(
	]]
		headerFile = headerFile .. ReadFile(path .. fileName)
		headerFile = headerFile .. [[

	)LUAFILE";]]
		
		WriteFile(headerFileName, headerFile)
	end
end
CompileLuaScipts()

local function CompileModuleList()
	local path = "../modules/"
	local moduleList = {}
	for _, fileName in ipairs(ScanDir(path, false)) do
		if not EndsWith(fileName, ".cpp") then continue end

		local content = ReadFile(path .. fileName)
		for moduleName in content:gmatch("IModule%s*%*%s*(%w+)%s*=%s*&%s*[%w_]+%s*") do
			table.insert(moduleList, moduleName)
		end
	end
	
	local moduleFile = [[
// This file is generated! Do NOT touch this!
#ifndef _MODULELIST_H
#define _MODULELIST_H
#include "module.h"

]]
	for _, moduleName in ipairs(moduleList) do
		moduleFile = moduleFile .. [[extern IModule* ]] .. moduleName .. [[;
]]
	end

	moduleFile = string.Trim(moduleFile) .. [[


#define HOLYLIB_MODULE_COUNT ]] .. tostring(#moduleList) .. [[


void CModuleManager::LoadModules()
{
]]

	for _, moduleName in ipairs(moduleList) do
		moduleFile = moduleFile .. [[	RegisterModule(]] .. moduleName .. [[);
]]
	end

	moduleFile = string.Trim(moduleFile) .. [[

}
#endif]]

	WriteFile(path .. "_modules.h", string.Trim(moduleFile))
end
CompileModuleList()