workspace "Bootil"
	configurations { "Debug", "Release" }
	platforms { "x32", "x64" }
	targetdir ( "%{cfg.shortname}_%{cfg.system}" )
	
	filter "platforms:x32"
		architecture "x32"

	filter "platforms:x64"
		architecture "x64"
	
	include( "bootil_premake5.lua" );
