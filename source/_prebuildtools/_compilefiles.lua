--[[
	This file is called as a prebuild command to generate all files we need
	though generating files can cause caching to fail if the contents change. (like in _versioninfo.h)
]]

dofile("utils.lua")

local function CompileLuaScripts()
	local path = "../lua/scripts/"
	for _, fileName in ipairs(ScanDir(path, false)) do
		if not EndsWith(fileName, ".lua") then continue end

		local headerFileName = path .. RemoveEnd(fileName, ".lua") .. ".h"
		local headerFile = [[const char* lua]] .. RemoveEnd(fileName, ".lua") .. [[ = R"LUAFILE(]]
		headerFile = headerFile .. string.Trim(ReadFile(path .. fileName)) -- Let's trim it to not include any new lines from the start/end of the file
		headerFile = headerFile .. [[

)LUAFILE";]]
		
		WriteFile(headerFileName, headerFile)
	end
end
CompileLuaScripts()

local function CompileModuleList()
	local path = "../modules/"
	local moduleList = {}
	for _, fileName in ipairs(ScanDir(path, false)) do
		if not EndsWith(fileName, ".cpp") then continue end

		local content = ReadFile(path .. fileName)
		for moduleName in content:gmatch("IModule%s*%*%s*(%w+)%s*=%s*&%s*[%w_]+%s*") do
			table.insert(moduleList, {moduleName, string.upper(RemoveEnd(fileName, ".cpp"))})
		end
	end
	
	local moduleFile = [[
// This file is generated! Do NOT touch this!
#ifndef _MODULELIST_H
#define _MODULELIST_H
#include "module.h"

]]
	--[[
		IMPORTANT NOTE: The ModuleID is based off the design that the ID 0 is reserved by the core
		So it starts at 1 for the first module which is why we can pass index directly.
		Why will the ID be accurate? Because the IDs are handed out in RegisterModule order - and they'll be the same order
	]]
	for index, moduleName in ipairs(moduleList) do
		moduleFile = moduleFile .. [[extern IModule* ]] .. moduleName[1] .. [[;
#define MODULE_EXISTS_]] .. moduleName[2] .. [[ 1
constexpr int HOLYLIB_MODULEID_]] .. moduleName[2] .. [[ = ]] .. index .. [[;

]]
	end

	moduleFile = string.Trim(moduleFile) .. [[


constexpr int HOLYLIB_MODULE_COUNT = ]] .. tostring(#moduleList) .. [[;
#endif]]

	WriteFile(path .. "_modules.h", string.Trim(moduleFile))

	local moduleCPPFile = [[#include "_modules.h"

void CModuleManager::LoadModules()
{
]]
	for _, moduleName in ipairs(moduleList) do
		moduleCPPFile = moduleCPPFile .. [[	RegisterModule(]] .. moduleName[1] .. [[);
]]
	end

	moduleCPPFile = moduleCPPFile .. [[
}]]

	WriteFile(path .. "_modules.cpp", string.Trim(moduleCPPFile))
end
CompileModuleList()


local function CompileVersionFile()
	local path = "../../workflow_info.txt"
	local file = io.open(path, "r")
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
CompileVersionFile()