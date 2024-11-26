-- Defines which version of the project generator to use, by default
-- (can be overriden by the environment variable PROJECT_GENERATOR_VERSION)
PROJECT_GENERATOR_VERSION = 3

newoption({
	trigger = "gmcommon",
	description = "Sets the path to the garrysmod_common (https://github.com/danielga/garrysmod_common) directory",
	default = "../garrysmod_common"
})

local gmcommon = assert(_OPTIONS.gmcommon or os.getenv("GARRYSMOD_COMMON"),
	"you didn't provide a path to your garrysmod_common (https://github.com/danielga/garrysmod_common) directory")
include(gmcommon)

local file = io.open("workflow_info.txt", "r") -- Added this file to the workflow so it could also be useful for others.
local run_id = file and file:read("*l") or "1" -- First like = workflow run id
local run_number = file and file:read("*l") or "1" --- Second line = workflow run number
local branch = file and file:read("*l") or "main" -- Third line = branch -> "main"
local additional = file and file:read("*l") or "0" -- Fouth line = Additional data. We set it to 1 for releases.

CreateWorkspace({name = "holylib", abi_compatible = false})
	-- Serverside module (gmsv prefix)
	-- Can define "source_path", where the source files are located
	-- Can define "manual_files", which allows you to manually add files to the project,
	-- instead of automatically including them from the "source_path"
	-- Can also define "abi_compatible", for project specific compatibility
	CreateProject({serverside = true, manual_files = false})
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
		--IncludeSDKMathlib()
		--IncludeSDKRaytrace()
		--IncludeSDKBitmap()
		--IncludeSDKVTF()
		IncludeSteamAPI()
		IncludeDetouring()
		IncludeScanning()

		-- I don't care about the ID.
		defines("GITHUB_RUN_NUMBER=\"" .. run_number .. "\"")
		defines("GITHUB_RUN_BRANCH=\"" .. branch .. "\"")
		defines("GITHUB_RUN_DATA=" .. additional)
		defines("SWDS=1")

		files({
			[[source/modules/*.h]],
			[[source/modules/*.cpp]],
			[[source/sourcesdk/*.h]],
			[[source/sourcesdk/*.cpp]],
			[[source/public/*.h]],
		})

		includedirs({
			[[Bootil/include/]],
			[[source/sourcesdk/]]
		})

		filter("system:windows")
			files({"source/win32/*.cpp", "source/win32/*.hpp"})
			links({"bootil_static_32.lib"})
			links({"bootil_static_64.lib"}) -- one of these will work :^
			links({"bass_32.lib"})
			links({"bass_64.lib"})

		filter("system:windows", "platforms:x86")
			libdirs("libs/win32")

		filter("system:windows", "platforms:x86_64")
			libdirs("libs/win64")

		filter({"system:linux", "platforms:x86_64"})
			libdirs("libs/linux64")

		filter({"system:linux", "platforms:x86"})
			libdirs("libs/linux32")

		filter("system:linux")
			targetextension(".so")
			links -- this fixes the undefined reference to `dlopen' errors.
				{
					"dl",
					"tier0",
					"pthread",
					"bass",
					"bootil_static"
				}

		filter("system:linux or macosx")
			files({"source/posix/*.cpp", "source/posix/*.hpp"})