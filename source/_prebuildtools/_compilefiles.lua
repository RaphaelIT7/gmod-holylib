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

local existingModules = {}
local function CompileModuleList()
	local path = "../modules/"
	local moduleList = {}
	for _, fileName in ipairs(ScanDir(path, false)) do
		if not EndsWith(fileName, ".cpp") then continue end

		local content = ReadFile(path .. fileName)
		for moduleName in content:gmatch("IModule%s*%*%s*(%w+)%s*=%s*&%s*[%w_]+%s*") do
			table.insert(moduleList, {
				moduleName,
				string.upper(RemoveEnd(fileName, ".cpp")),
				-- If set this module will get a higher ID & will be loaded sooner
				priority = string.find(content, "HOLYLIB_PRIORITY_MODULE") ~= nil,
			})
		end
	end

	table.sort(moduleList, function(a, b)
		if a.priority ~= b.priority then
			return a.priority and not b.priority
		end
		return a[1] < b[1]
	end)
	
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
		existingModules[moduleName[2]] = true
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

local function ExecuteModuleMacros()
	local path = "../modules/"
	local moduleList = {}
	for _, fileName in ipairs(ScanDir(path, false)) do
		if not EndsWith(fileName, ".cpp") then continue end

		local processState = {
			mode = "none",
			phase = "none",
			outputFile = nil,
			buffer = {},
			replacePerLine = nil,
			dependsOnModule = nil,
		}

		local content = OpenFile(path .. fileName, "rb")
		for line in content:lines() do
			if line:match("HOLYLIB_SETUP_FILE=([^\r\n]+)") then
				if processState.mode ~= "none" then
					error("Process state wasn't complete yet!")
				end

				processState.mode = "generateFile"
				processState.outputFile = string.Trim(line:match("HOLYLIB_SETUP_FILE=([^\r\n]+)"))
			elseif line:match("HOLYLIB_SETUP_FILE_END") then
				if processState.replacePerLine then
					local rep = processState.replacePerLine
					for idx, line in ipairs(processState.buffer) do
						processState.buffer[idx] = string.Replace(line, rep.from, rep.to)
					end
				end

				if not processState.dependsOnModule or existingModules[string.upper(processState.dependsOnModule)] then
					WriteFile("../" .. processState.outputFile, table.concat(processState.buffer, NewLine))
				end

				processState.mode = "none"
			elseif line:match("HOLYLIB_SETUP_FILE_CONTENTS_BEGIN") then
				processState.phase = "content"
			elseif line:match("HOLYLIB_SETUP_FILE_CONTENTS_END") then
				processState.phase = "none"
			elseif line:match("HOLYLIB_SETUP_FILE_DEPENDING_MODULE=([^\r\n]+)") then
				processState.dependsOnModule = string.Trim(line:match("HOLYLIB_SETUP_FILE_DEPENDING_MODULE=([^\r\n]+)"))
			elseif line:match("HOLYLIB_SETUP_FILE_REPLACE_PER_LINE=([^\r\n]+)") then
				local info = string.Trim(line:match("HOLYLIB_SETUP_FILE_REPLACE_PER_LINE=([^\r\n]+)"))
				local middle, middle_end = string.find(info, "==>")
				if middle then
					processState.replacePerLine = {
						from = string.sub(info, 0, middle-1),
						to = string.sub(info, middle_end+1),
					}
				end
			elseif line:match("HOLYLIB_SETUP_FILE_SKIPNEXTLINE") then
				processState.phase = "skipLine"
			else
				if processState.phase == "skipLine" then
					processState.phase = "content"
				elseif processState.phase == "content" then
					table.insert(processState.buffer, line)
				end
			end
		end
	end
end
ExecuteModuleMacros()

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