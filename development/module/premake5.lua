-- Defines which version of the project generator to use, by default
-- (can be overridden by the environment variable PROJECT_GENERATOR_VERSION)
PROJECT_GENERATOR_VERSION = 3

newoption({
	trigger = "gmcommon",
	description = "Sets the path to the garrysmod_common (https://github.com/danielga/garrysmod_common) directory",
	default = io.open("../../../fork-garrysmod_common/license.txt", "r") and "../../../fork-garrysmod_common" or "../garrysmod_common"
})

HOLYLIB_DEVELOPMENT = true
load(io.readfile("../../premake5.lua"))()