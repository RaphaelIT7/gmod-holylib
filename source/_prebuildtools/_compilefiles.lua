dofile("utils.lua")

local function CompileLuaScipts()
	local path = "../lua/scripts/"
	for _, fileName in ipairs(ScanDir(path, false)) do
		if not EndsWith(fileName, ".lua") then continue end

		local headerFileName = path .. RemoveEnd(fileName, ".lua") .. ".h"
		local headerFile = [[const char* lua]] .. RemoveEnd(fileName, ".lua") .. [[ = R"LUAFILE(
]]
		headerFile = headerFile .. string.Trim(ReadFile(path .. fileName)) -- Let's trim it to not include any new lines from the start/end of the file
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


local function CompileVerionFile()
	local path = "../../workflow_info.txt"
	local file = io.open("workflow_info.txt", "r")
	local run_id = file and file:read("*l") or "1"
	local run_number = file and file:read("*l") or "1"
	local branch = file and file:read("*l") or "main"
	--local additional = file and file:read("*l") or "0"

	local versionFile = [[
// This is a generated file! & This will change on every run and don't include it unless you want cache misses & compiles to take ages.
#pragma once

#ifndef _ALLOWVERSIONFILE
#error "you included the wrong file! Use versioninfo.h, not _versioninfo.h!"
#endif

#define HOLYLIB_BUILD_BRANCH "]] .. branch .. [["
#define HOLYLIB_BUILD_RUN_NUMBER "]] .. run_number .. [["
]]

	WriteFile("../_versioninfo.h", string.Trim(versionFile))
end
CompileVerionFile()