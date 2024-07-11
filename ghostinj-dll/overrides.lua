--[[
    CreateWorkspace Override. 
    This just removed the gmsv_ from the project name.
]]
function CreateWorkspace(config)
    assert(type(config) == "table", "supplied argument is not a table!")

    local name = config.name
    assert(type(name) == "string", "'name' is not a string!")

    local directory = config.path or _OPTIONS["workspace"] or WORKSPACE_DIRECTORY
    assert(type(directory) == "string", "workspace path is not a string!")

    local _workspace = workspace(name)
    assert(_workspace.directory == nil, "a workspace with the name '" .. name .. "' already exists!")

    local abi_compatible
    if config.allow_debug ~= nil then
        assert(type(config.allow_debug) == "boolean", "'allow_debug' is not a boolean!")
        print("WARNING: The 'allow_debug' option has been deprecated in favor of 'abi_compatible' (same functionality, better name, takes precedence over 'allow_debug', allows setting per project where the workspace setting takes precedence if set to true)")
        abi_compatible = not config.allow_debug
    end

    if config.abi_compatible ~= nil then
        abi_compatible = config.abi_compatible
        assert(type(abi_compatible) == "boolean", "'abi_compatible' is not a boolean!")
        _workspace.abi_compatible = abi_compatible
    end

    _workspace.directory = directory

        language("C++")
        location(_workspace.directory)
        warnings("Extra")
        flags({"MultiProcessorCompile", "ShadowedVariables", "UndefinedIdentifiers"})
        characterset("MBCS")
        intrinsics("On")
        inlining("Auto")
        rtti("On")
        vectorextensions("SSE2")
        targetdir("%{wks.location}/%{cfg.architecture}/%{cfg.buildcfg}")
        debugdir("%{wks.location}/%{cfg.architecture}/%{cfg.buildcfg}")
        objdir("!%{wks.location}/%{cfg.architecture}/%{cfg.buildcfg}/intermediate/%{prj.name}")

        if PROJECT_GENERATOR_VERSION < 3 then
            platforms("x86")
        else
            platforms({"x86_64", "x86"})
        end

        if abi_compatible then
            configurations({"ReleaseWithSymbols", "Release"})

            filter("system:linux or macosx")
                defines("_GLIBCXX_USE_CXX11_ABI=0")
        else
            configurations({"ReleaseWithSymbols", "Release", "Debug"})
        end

        filter("platforms:x86_64")
            architecture("x86_64")

        filter("platforms:x86")
            architecture("x86")

        filter("configurations:ReleaseWithSymbols")
            optimize("Debug")
            symbols("On")
            defines("NDEBUG")
            runtime("Release")

        filter("configurations:Release")
            flags("LinkTimeOptimization")
            optimize("Full")
            symbols("Off")
            defines("NDEBUG")
            runtime("Release")

        if not abi_compatible then
            filter("configurations:Debug")
                optimize("Off")
                symbols("On")
                defines({"DEBUG", "_DEBUG"})
                runtime("Debug")
        end

        filter("system:windows")
            cdialect("C11")
            cppdialect("C++17")
            staticruntime("On")
            defaultplatform("x86")
            defines({
                "_CRT_NONSTDC_NO_WARNINGS",
                "_CRT_SECURE_NO_WARNINGS",
                "STRICT"
            })

        filter("system:linux")
            cdialect("GNU11")
            cppdialect("GNU++17")
            pic("On")
            staticruntime("On")
            defaultplatform("x86")
            linkoptions("-Wl,--no-undefined")

        filter("system:macosx")
            cdialect("GNU11")
            cppdialect("GNU++17")
            pic("On")
            staticruntime("Off")
            defaultplatform("x86_64")
            buildoptions({"-mmacosx-version-min=10.7", "-stdlib=libc++"})
            linkoptions({"-mmacosx-version-min=10.7", "-stdlib=libc++"})

            local macosx_sdkroot = _OPTIONS["macosx_sdkroot"]
            if macosx_sdkroot ~= nil then
                buildoptions("-isysroot " .. macosx_sdkroot)
                linkoptions("-isysroot " .. macosx_sdkroot)
            end

        filter({"language:C++", "system:windows or linux"})
            strictaliasing("Level3")

        filter({})
end

local function GetSteamLibraryDirectories()
    local dir

    if os.istarget("windows") then
        local regPath = os.getWindowsRegistry("HKCU:\\Software\\Valve\\Steam\\SteamPath")
        if regPath then
            dir = path.join(regPath, "SteamApps")
        else
            local p = io.popen("wmic logicaldisk get caption")

            for line in p:read("*a"):gmatch("%S+") do
                if line ~= "Caption" then
                    local steamDir1 = path.join(line, "Program Files (x86)", "Steam", "SteamApps")
                    local steamDir2 = path.join(line, "Program Files", "Steam", "SteamApps")

                    if os.isdir(steamDir1) then
                        dir = steamDir1
                    elseif os.isdir(steamDir2) then
                        dir = steamDir2
                    end
                end
            end

            p:close()
        end
    elseif os.istarget("linux") then
        dir = path.join(os.getenv("HOME") or "~", ".local", "share", "Steam", "SteamApps")
    elseif os.istarget("macosx") then
        dir = path.join(os.getenv("HOME") or "~", "Library", "Application Support", "Steam", "SteamApps")
    end

    if dir then
        local dirs = {dir}

        if os.isfile(path.join(dir, "libraryfolders.vdf")) then
            local f = io.open(path.join(dir, "libraryfolders.vdf"), "r")

            for _, libdir in f:read("*a"):gmatch("\n%s*\"(%d+)\"%s*\"(.-)\"") do
                if os.isdir(libdir) then
                    local sappsPath = path.join(libdir, "steamapps")
                    if os.isdir(sappsPath) then
                        dirs[#dirs + 1] = sappsPath
                    end
                end
            end

            f:close()
        end

        return dirs
    end

    return {}
end

local function FindGarrysModDirectory()
    local dirs = GetSteamLibraryDirectories()

    for _, dir in ipairs(dirs) do
        if os.isdir(path.join(dir, "common", "GarrysMod")) then
            return path.join(dir, "common", "GarrysMod")
        elseif os.isdir(path.join(dir, "common", "garrysmod")) then
            return path.join(dir, "common", "garrysmod")
        end
    end

    return
end

local function FindGarrysModLuaBinDirectory()
    local dir = FindGarrysModDirectory()
    if not dir then
        return
    end

    local gluabinPath = path.join(dir, "garrysmod", "lua", "bin")
    if not os.isdir(gluabinPath) then
        os.mkdir(gluabinPath)
    end

    return gluabinPath
end

local startproject_defined = false
function CreateProject(config)
    assert(type(config) == "table", "supplied argument is not a table!")

    local is_server = config.serverside
    assert(type(is_server) == "boolean", "'serverside' option is not a boolean!")

    local is_vs = string.find(_ACTION, "^vs20%d%d$") ~= nil

    local sourcepath = config.source_path or _OPTIONS["source"] or SOURCE_DIRECTORY
    assert(type(sourcepath) == "string", "source code path is not a string!")

    local manual_files = config.manual_files
    if manual_files == nil then
        manual_files = false
    else
        assert(type(manual_files) == "boolean", "'manual_files' is not a boolean!")
    end

    local _workspace = workspace()

    local abi_compatible = _workspace.abi_compatible
    if not abi_compatible then
        if config.abi_compatible ~= nil then
            abi_compatible = config.abi_compatible
            assert(type(abi_compatible) == "boolean", "'abi_compatible' is not a boolean!")
        else
            abi_compatible = false
        end
    end

    local pch_enabled = false
    if config.pch_header ~= nil or config.pch_source ~= nil then
        assert(config.pch_header ~= nil, "'phc_header' must be supplied when 'pch_source' is supplied!")
        assert(type(config.pch_header) == "string", "'pch_header' is not a string!")

        if is_vs then
            assert(config.pch_source ~= nil, "'pch_source' must be supplied when 'phc_header' is supplied under Visual Studio!")
            assert(type(config.pch_source) == "string", "'pch_source' is not a string!")

            config.pch_source = sourcepath .. "/" .. config.pch_source
            assert(os.isfile(config.pch_source), "'pch_source' file " .. config.pch_source .. " could not be found!")
        end

        pch_enabled = true
    end

    local name = (config.gmprefix and (is_server and "gmsv_" or "gmcl_") or "") .. (config.name or _workspace.name)

    local windows_actions = {
        vs2015 = true,
        vs2017 = true,
        vs2019 = true,
        vs2022 = true,
        install = true,
        clean = true,
        lint = true,
        format = true,
        ["export-compile-commands"] = true
    }
    if abi_compatible and os.istarget("windows") and not windows_actions[_ACTION] then
        error("The only supported compilation platforms for this project (" .. name .. ") on Windows are Visual Studio 2015, 2017, 2019 and 2022.")
    end

    if not startproject_defined then
        startproject(name)
        startproject_defined = true
    end

    local _project = project(name)

    assert(_project.directory == nil, "a project with the name '" .. name .. "' already exists!")

    _project.directory = sourcepath
    _project.serverside = is_server

        if abi_compatible then
            removeconfigurations("Debug")
            configurations({"ReleaseWithSymbols", "Release"})
        else
            configurations({"ReleaseWithSymbols", "Release", "Debug"})
        end

        if pch_enabled then
            pchheader(config.pch_header)
            if is_vs then
                pchsource(config.pch_source)
            end
        else
            flags("NoPCH")
        end

        kind("SharedLib")
        language("C++")
        defines({
            "GMMODULE",
            string.upper(string.gsub(_workspace.name, "%.", "_")) .. (_project.serverside and "_SERVER" or "_CLIENT"),
            "IS_SERVERSIDE=" .. tostring(is_server),
            "GMOD_ALLOW_OLD_TYPES",
            "GMOD_ALLOW_LIGHTUSERDATA",
            "GMOD_MODULE_NAME=\"" .. _workspace.name .. "\""
        })
        externalincludedirs(_GARRYSMOD_COMMON_DIRECTORY .. "/include")
        includedirs(_project.directory)

        if not manual_files then
            files({
                _project.directory .. "/*.h",
                _project.directory .. "/*.hpp",
                _project.directory .. "/*.hxx",
                _project.directory .. "/*.c",
                _project.directory .. "/*.cpp",
                _project.directory .. "/*.cxx"
            })
        end

        vpaths({
            ["Header files/*"] = {
                _project.directory .. "/**.h",
                _project.directory .. "/**.hpp",
                _project.directory .. "/**.hxx"
            },
            ["Source files/*"] = {
                _project.directory .. "/**.c",
                _project.directory .. "/**.cpp",
                _project.directory .. "/**.cxx"
            }
        })

        if abi_compatible then
            local filepath = _GARRYSMOD_COMMON_DIRECTORY .. "/source/ABICompatibility.cpp"
            files(filepath)
            vpaths({["garrysmod_common"] = filepath})
        end

        targetprefix("")

        local autoinstall = _OPTIONS["autoinstall"]
        if autoinstall ~= nil then
            local binDir = #autoinstall ~= 0 and autoinstall or os.getenv("GARRYSMOD_LUA_BIN") or FindGarrysModLuaBinDirectory() or GARRYSMOD_LUA_BIN_DIRECTORY
            assert(type(binDir) == "string", "The path to garrysmod/lua/bin is not a string!")

            filter({})
                postbuildcommands({"{COPY} \"%{cfg.buildtarget.abspath}\" \"" .. binDir .. "\""})
        end

        IncludeHelpers()

        filter({})
end