-- Defines which version of the project generator to use, by default
-- (can be overriden by the environment variable PROJECT_GENERATOR_VERSION)
PROJECT_GENERATOR_VERSION = 3

newoption({
	trigger = "gmcommon",
	description = "Sets the path to the garrysmod_common (https://github.com/danielga/garrysmod_common) directory",
	default = io.open("../../fork-garrysmod_common/license.txt", "r") and "../../fork-garrysmod_common" or "../garrysmod_common"
})

local gmcommon = assert(_OPTIONS.gmcommon or os.getenv("GARRYSMOD_COMMON"),
	"you didn't provide a path to your garrysmod_common (https://github.com/danielga/garrysmod_common) directory")
include(gmcommon)

CreateWorkspace({name = "exampledll", abi_compatible = false})
	-- Serverside module (gmsv prefix)
	-- Can define "source_path", where the source files are located
	-- Can define "manual_files", which allows you to manually add files to the project,
	-- instead of automatically including them from the "source_path"
	-- Can also define "abi_compatible", for project specific compatibility
	CreateProject({serverside = true, manual_files = false})
		kind "SharedLib"
		symbols "On"
		-- Remove some or all of these includes if they're not needed
		--IncludeHelpersExtended()
		--IncludeLuaShared()
		--IncludeSDKEngine()
		IncludeSDKCommon()
		--IncludeSDKTier0()
		IncludeSDKTier1()
		--IncludeSDKTier2()
		--IncludeSDKTier3()
		--IncludeSDKMathlib()
		--IncludeSDKRaytrace()
		--IncludeSDKBitmap()
		--IncludeSDKVTF()
		--IncludeSteamAPI()
		--IncludeDetouring()
		--IncludeScanning()

		filter("system:windows")
			files({"source/win32/*.cpp", "source/win32/*.hpp"})

		filter("system:linux")
			targetextension(".dll")
			links("dl") -- this fixes the undefined reference to `dlopen' errors.