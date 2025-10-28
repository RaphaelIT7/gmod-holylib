--[[
	This premake file is also loaded by our development/module/premake5.lua
	If this happens the HOLYLIB_DEVELOPMENT will be set to true
	I did this to have 1 premake file instead of 2 seperate ones.
]]
PROJECT_GENERATOR_VERSION = 3

if not HOLYLIB_DEVELOPMENT then
	newoption({
		trigger = "gmcommon",
		description = "Sets the path to the garrysmod_common (https://github.com/danielga/garrysmod_common) directory",
		default = "../garrysmod_common"
	})
end

local gmcommon = assert(_OPTIONS.gmcommon or os.getenv("GARRYSMOD_COMMON"),
	"you didn't provide a path to your garrysmod_common (https://github.com/danielga/garrysmod_common) directory")
include(gmcommon)

local rootDir = ""
local sourcePath = "source/"
if HOLYLIB_DEVELOPMENT then
	rootDir = "../../"
	sourcePath = rootDir .. sourcePath
end

if os.isdir(sourcePath .. "ivp/") then -- Anyone can remove the ivp folder if they don't want HolyLib to include a IVP build.
	include(sourcePath .. "ivp/premake5.lua")
end

include(sourcePath .. "bootil/premake5.lua")

include(sourcePath .. "_prebuildtools/_dependency_manager.lua")

--[[
	Workflow info variables
]]
local file = io.open("workflow_info.txt", "r") -- Added this file to the workflow so it could also be useful for others.
local run_id = file and file:read("*l") or "1" -- First line = workflow run id
local run_number = file and file:read("*l") or "1" --- Second line = workflow run number
local branch = file and file:read("*l") or "main" -- Third line = branch -> "main"
local additional = file and file:read("*l") or "0" -- Fouth line = Additional data. We set it to 1 for releases.

--[[
	Prebuild command setup
]]
local prebuildCommand = ""
local basePath = "cd ../../../"
if os.host() == "windows" then
	prebuildCommand = sourcePath .. "_prebuildtools/ && luajit.exe _compilefiles.lua"
else
	if os.is64bit() then
		prebuildCommand = sourcePath .. "_prebuildtools/ && chmod +x luajit_64 && ./luajit_64 _compilefiles.lua"
	else
		prebuildCommand = sourcePath .. "_prebuildtools/ && chmod +x luajit_32 && ./luajit_32 _compilefiles.lua"
	end
end

os.execute("cd " .. prebuildCommand)
prebuildCommand = basePath .. prebuildCommand -- We add this after execute since the path for premake setup & project build differ

--[[
	Project setup
]]
CreateWorkspace({name = "holylib", abi_compatible = false})
	-- Serverside module (gmsv prefix)
	-- Can define "source_path", where the source files are located
	-- Can define "manual_files", which allows you to manually add files to the project,
	-- instead of automatically including them from the "source_path"
	-- Can also define "abi_compatible", for project specific compatibility
	CreateProject({serverside = true, manual_files = false, source_path = sourcePath:sub(0, -2)})
		kind "SharedLib"
		symbols "On"

		-- Remove some or all of these includes if they're not needed
		IncludeHelpersExtended()
		--IncludeLuaShared()
		IncludeSDKCommon()
		IncludeSDKTier0()
		IncludeSDKTier1()
		--IncludeSDKTier2()
		--IncludeSDKTier3()
		IncludeSDKMathlib()
		--IncludeSDKRaytrace()
		--IncludeSDKBitmap()
		--IncludeSDKVTF()
		IncludeSteamAPI()
		IncludeDetouring()
		IncludeScanning()
		if IncludeIVP then
			IncludeIVP() -- Can be removed if anyone doesn't want it.
		end
		IncludeBootil()

		if HOLYLIB_DEVELOPMENT then
			defines("HOLYLIB_BUILD_RELEASE=0")
			defines("HOLYLIB_DEVELOPMENT=1")
		else
			defines("HOLYLIB_BUILD_RELEASE=" .. additional)
		end
		defines("SWDS=1")
		defines("PROJECT_NAME=\"holylib\"")
		defines("NO_FRAMESNAPSHOTDEF")
		defines("NO_VCR")
		defines("IVP_NO_PERFORMANCE_TIMER")
		defines("PHYSENV_INCLUDEIVPFALLBACK")
		defines("CPPHTTPLIB_NO_EXCEPTIONS") -- We don't want exceptions!

		prebuildcommands(prebuildCommand)

		files({
			gmcommon .. [[/sourcesdk-minimal/public/filesystem_helpers.cpp]],
			sourcePath .. [[opus/*.h]],
			sourcePath .. [[modules/*.h]],
			sourcePath .. [[modules/*.cpp]],
			sourcePath .. [[sourcesdk/*.h]],
			sourcePath .. [[sourcesdk/*.cpp]],
			sourcePath .. [[public/*.h]],
			sourcePath .. [[lua/*.*]],
			sourcePath .. [[lz4/*.h]],
			sourcePath .. [[lz4/*.c]],
			sourcePath .. [[lz4/*.cpp]],

			rootDir .. "lua/*.h",
			rootDir .. "source/lua/scripts/*.lua",
			rootDir .. "README.md",
			rootDir .. ".github/workflows/**.yml",
		})

		vpaths({
			["Source files/sourcesdk/"] = gmcommon .. "/**.*",
			["Lua Headers"] = rootDir .. "lua/*.h",
			["Lua Scrips"] = rootDir .. "source/lua/scripts/*.lua",
			["README"] = rootDir .. "README.md",
			["Workflows"] = rootDir .. ".github/workflows/**.yml",
		})

		removefiles(GetExcludedFiles(sourcePath)) -- Remove source files marked by our dependency manager

		includedirs({
			sourcePath .. [[sourcesdk/]],
			sourcePath .. [[lua]]
		})

		filter("system:windows")
			defines("IVP_NO_MATH_INL")
			disablewarnings({"4101"})
			links({"lua51_32.lib"})
			links({"lua51_64.lib"})
			links({"bass_32.lib"})
			links({"bass_64.lib"})
			links({"opus_32.lib"})
			links({"opus_64.lib"})

		filter("system:windows", "platforms:x86")
			libdirs(rootDir .. "libs/win32")

		filter("system:windows", "platforms:x86_64")
			libdirs(rootDir .. "libs/win64")

		filter({"system:linux", "platforms:x86_64"})
			libdirs(rootDir .. "libs/linux64")
			buildoptions({"-mcx16"}) -- Should solve this: undefined reference to `__sync_bool_compare_and_swap_16'
			links("luajit_64")
			links("opus_64")

		filter({"system:linux", "platforms:x86"})
			libdirs(rootDir .. "libs/linux32")
			links("luajit_32")
			links("opus_32")

		filter({"platforms:x86_64"})
			defines("PLATFORM_64BITS")

		filter("system:linux")
			disablewarnings({"unused-variable"})
			targetextension(".so")
			links({"dl", "tier0", "pthread", "bass"}) -- this fixes the undefined reference to `dlopen' errors.
			defines("DEDICATED") -- All linux build focus Linux dedicated servers. Windows focus on the gmod client