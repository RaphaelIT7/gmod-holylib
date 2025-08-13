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

CreateWorkspace({name = "holylib", abi_compatible = false})
	-- Serverside module (gmsv prefix)
	-- Can define "source_path", where the source files are located
	-- Can define "manual_files", which allows you to manually add files to the project,
	-- instead of automatically including them from the "source_path"
	-- Can also define "abi_compatible", for project specific compatibility

	CreateProject({serverside = true, manual_files = false, source_path = "../../source"})
		kind "SharedLib"
		symbols "On"
		-- enableunitybuild "On" -- Caused 500+ errors :/ (We need to use the newest premake5 build to use this, the workflows DONT have this version!)
		
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

		defines("HOLYLIB_BUILD_RELEASE=0") -- No release builds.
		defines("HOLYLIB_DEVELOPMENT=1")
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
			[[../../.github/workflows/**.yml]],
		})

		vpaths({
			["Source files/sourcesdk/"] = gmcommon .. "/**.*",
			["Lua Headers"] = "../../lua/*.h",
			["Lua Scrips"] = "../../source/lua/scripts/*.lua",
			["README"] = "../../README.md",
			["Workflows"] = "../../.github/workflows/**.yml",
		})

		includedirs({
			[[../../source/sourcesdk/]],
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
				"cd ../../../../../source/_prebuildtools/ && luajit.exe _compilefiles.lua"
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
				"cd ../../../../../source/_prebuildtools/ && chmod +x luajit_64 && ./luajit_64 _compilefiles.lua"
			})

		filter({"system:linux", "platforms:x86"})
			libdirs("../../libs/linux32")
			links("luajit_32")
			links("opus_32")

			prebuildcommands({
				"cd ../../../../../source/_prebuildtools/ && chmod +x luajit_32 && ./luajit_32 _compilefiles.lua"
			})

		filter({"platforms:x86_64"})
			defines("PLATFORM_64BITS")

		filter("system:linux")
			targetextension(".so")
			links({"dl", "tier0", "pthread", "bass"}) -- this fixes the undefined reference to `dlopen' errors.