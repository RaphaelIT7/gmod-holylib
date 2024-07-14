project "bootil_static"

	uuid ( "AB8E7B19-A70C-4767-88DE-F02160167C2E" )
	defines { "BOOTIL_COMPILE_STATIC", "BOOST_ALL_NO_LIB" }
	files { "../src/**.cpp", "../include/**.h", "../src/**.c", "../src/**.cc", "premake4.lua" }
	kind "StaticLib"
	targetname( "bootil_static" )
	flags { "NoPCH" }
	includedirs { "../include/", "../src/3rdParty/" }

	exceptionhandling "SEH"
	symbols "On"
	vectorextensions "SSE"
	staticruntime "On"
	editandcontinue "Off"
	runtime "Release"

	filter { "platforms:x32" }
        targetname "bootil_static_32"

    filter { "platforms:x64" }
        targetname "bootil_static_64"

	if os.is( "linux" ) or os.is( "macosx" ) then
		targetextension(".a")
		buildoptions { "-fPIC" }
	end

	if os.is( "windows" ) then
		characterset "MBCS"
	end

