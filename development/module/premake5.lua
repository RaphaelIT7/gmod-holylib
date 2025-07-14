-- Defines which version of the project generator to use, by default
-- (can be overriden by the environment variable PROJECT_GENERATOR_VERSION)
PROJECT_GENERATOR_VERSION = 3

newoption({
	trigger = "gmcommon",
	description = "Sets the path to the garrysmod_common (https://github.com/danielga/garrysmod_common) directory",
	default = io.open("../../../fork-garrysmod_common/license.txt", "r") and "../../../fork-garrysmod_common" or "../garrysmod_common"
})

local gmcommon = assert(_OPTIONS.gmcommon or os.getenv("GARRYSMOD_COMMON"),
	"you didn't provide a path to your garrysmod_common (https://github.com/danielga/garrysmod_common) directory")
include(gmcommon)
include("../../source/ivp/premake5.lua")
include("../../source/bootil/premake5.lua")

--local file = io.open("../../workflow_info.txt", "r")
local run_id = "1"
local run_number = "1"
local branch = "main"
local additional = "0"

CreateWorkspace({name = "holylib", abi_compatible = false})
	-- Serverside module (gmsv prefix)
	-- Can define "source_path", where the source files are located
	-- Can define "manual_files", which allows you to manually add files to the project,
	-- instead of automatically including them from the "source_path"
	-- Can also define "abi_compatible", for project specific compatibility

	CreateProject({serverside = true, manual_files = false, source_path = "../../source"})
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
		IncludeIVP()
		IncludeBootil()

		-- I don't care about the ID.
		defines("GITHUB_RUN_NUMBER=\"" .. run_number .. "\"")
		defines("GITHUB_RUN_BRANCH=\"" .. branch .. "\"")
		defines("GITHUB_RUN_DATA=" .. additional)
		defines("SWDS=1")
		defines("PROJECT_NAME=\"holylib\"")
		defines("NO_FRAMESNAPSHOTDEF")
		defines("NO_VCR")
		defines("IVP_NO_MATH_INL")
		defines("IVP_NO_PERFORMANCE_TIMER")
		defines("PHYSENV_INCLUDEIVPFALLBACK")

		files({
			gmcommon .. [[/sourcesdk-minimal/public/filesystem_helpers.cpp]],
			[[../../source/opus/*.h]],
			[[../../source/modules/*.h]],
			[[../../source/modules/*.cpp]],
			[[../../source/sourcesdk/*.h]],
			[[../../source/sourcesdk/*.cpp]],
			[[../../source/public/*.h]],
			[[../../source/lua/*.*]],
			[[../../source/lua/scripts/*.lua]],
			[[../../source/lz4/*.h]],
			[[../../source/lz4/*.c]],
			[[../../source/lz4/*.cpp]],
			[[../../lua/*.h]],
			[[../../lua/*.hpp]],
			[[../../README.md]],
		})

		removefiles({
			[[../../source/modules/lagcompensation.cpp]] -- It's not finished yet.
		})

		includedirs({
			[[../../source/sourcesdk/]],
			[[../../lua/]],
			[[../../source/lua/]]
		})

		filter("system:windows")
			files({"source/win32/*.cpp", "source/win32/*.hpp"})
			links({"lua51_32.lib"})
			links({"lua51_64.lib"})
			links({"bass_32.lib"})
			links({"bass_64.lib"})
			links({"opus_32.lib"})
			links({"opus_64.lib"})

			prebuildcommands({
				"cd ../../../../../source/lua/scripts/ && luajit.exe _compilefiles.lua"
			})

		filter("system:windows", "platforms:x86")
			libdirs("../../libs/win32")

		filter("system:windows", "platforms:x86_64")
			libdirs("../../libs/win64")

		filter({"system:linux", "platforms:x86_64"})
			libdirs("../../libs/linux64")
			links("luajit_64")
			links("opus_64")

			prebuildcommands({
				"cd ../../../../../source/lua/scripts/ && chmod +x luajit_64 && ./luajit_64 _compilefiles.lua"
			})

		filter({"system:linux", "platforms:x86"})
			libdirs("../../libs/linux32")
			links("luajit_32")
			links("opus_32")

			prebuildcommands({
				"cd ../../../../../source/lua/scripts/ && chmod +x luajit_32 && ./luajit_32 _compilefiles.lua"
			})

		filter({"platforms:x86_64"})
			defines("PLATFORM_64BITS")

		filter("system:linux")
			targetextension(".so")
			links -- this fixes the undefined reference to `dlopen' errors.
				{
					"dl",
					"tier0",
					"pthread",
					"bass",
				}